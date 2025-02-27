//
// Created by diego on 10/30/24.
//
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LENGTH 35
#define RECIPE_TABLE_SIZE 500000
#define INGREDIENT_TABLE_SIZE 5000
#define MAX_INGREDIENT_RETRIES 25
#define MAX_RECIPE_RETRIES 35

// ****____****____****____****____**** STRUTTURE DATI ****____****____****____****____****

// Lotto: Quantità e scadenza
typedef struct {
    int quantity;
    int expiration;
} Lot;

// Nodo del Min-Heap per i lotti
typedef struct {
    Lot *lots;  // Array di lotti
    int size;   // Dimensione del min-heap
    int capacity; // Capacità massima del min-heap
} MinHeap_lots;

// Struttura per un ingrediente con nome e min-heap dei lotti
typedef struct {
    char *name;   // Nome dell'ingrediente
    MinHeap_lots *heap;  // Min-Heap dei lotti di quell'ingrediente
    int total_quantity;
} Ingredient;
//TODO per velocizzare potrei mettere puntatore al posto di hash
// Nodo per la lista di ingredienti di una ricetta
typedef struct IngredientNode {
    char *name;  // Nome dell'ingrediente
    unsigned int hash;
    int quantity;  // Quantità richiesta per la ricetta
    //struct IngredientNode *next;  // Puntatore al prossimo ingrediente
} IngredientNode;

// Ricetta: Nome e lista di ingredienti
typedef struct {
    char *name;   // Nome della ricetta
    int weight;  // Peso della ricetta
    int last_quantity_failed;
    int last_tick_check;
    int ingredients_size;
    IngredientNode *ingredients;  // array degli ingredienti richiesti
} Recipe;

// Nodo per la lista doppiamente collegata degli ordini
typedef struct OrderNode {
    Recipe *recipe;
    int quantity;  // Numero di elementi ordinati
    int tick; // Istante di arrivo dell'ordine
    struct OrderNode *prev;  // Puntatore al nodo precedente
    struct OrderNode *next;  // Puntatore al nodo successivo
} OrderNode;

// Struct per gestire una lista di ordini (ordini_in_attesa e ordini_pronti)
typedef struct OrderList {
    OrderNode *head;  // Testa della lista
    OrderNode *tail;  // Coda della lista
} OrderList;

// Min-Heap per gli ordini pronti
typedef struct{
    OrderNode **orders;  // Array di ordini
    int size;   // Dimensione del min-heap
    int capacity; // Capacità massima del min-heap
}MinHeap_orders;

// ****____****____****____****____**** FUNZIONI DI BASE ****____****____****____****____****

// Funzione di hashing FNV1a
#define FNV_OFFSET_BASIS 2166136261u  // Valore di offset iniziale per FNV-1a (32-bit)
#define FNV_PRIME 16777619u           // Primo utilizzato nella moltiplicazione

unsigned int xor(unsigned int hash, unsigned char value) {
    return hash ^ value;
}

unsigned int fnv1a_hash(const char *key) {
    unsigned int hash = FNV_OFFSET_BASIS;
    while (*key) {
        hash = xor(hash, *key); // Chiama la funzione XOR
        hash *= FNV_PRIME;      // Moltiplica per il primo
        key++;                  // Avanza alla lettera successiva
    }
    return hash;
}

// Crea un min-heap per i lotti
MinHeap_lots* create_minheap_lots(int capacity) {
    MinHeap_lots *minHeap = malloc(sizeof(MinHeap_lots));
    minHeap->size = 0;
    minHeap->capacity = capacity;
    minHeap->lots = (Lot*) malloc(capacity * sizeof(Lot));
    return minHeap;
}

// Crea un min-heap per gli ordini pronti
MinHeap_orders* create_minheap_orders(int capacity) {
    MinHeap_orders *heap = malloc(sizeof(MinHeap_orders));
    heap->orders = (OrderNode **)malloc(capacity * sizeof(OrderNode *));
    heap->size = 0;
    heap->capacity = capacity;
    return heap;
}

int string_to_int(const char *str) {
    int result = 0;
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}

// ****____****____****____****____**** VARIABILI GLOBALI ****____****____****____****____****

Ingredient *ingredientTable[INGREDIENT_TABLE_SIZE];     // Hash Table per gli ingredienti
Recipe *recipeTable[RECIPE_TABLE_SIZE];                 // Hash Table per le ricette
OrderList pending_orders_list = {NULL, NULL};  // Lista degli ordini in attesa
OrderList orders_to_load = {NULL, NULL};       // Lista degli ordini pronti da ordinare prima di spedizione
int last_supply_tick = -1;

// ****____****____****____****____**** GESTIONE INGREDIENTI ****____****____****____****____****

// Funzione per ridimensionare il Min-Heap dei lotti di un ingrediente
void resize_minheap(MinHeap_lots *heap) {
    if (heap->lots == NULL) return;
    heap->capacity *= 2;  // Raddoppia la capacità
    Lot *new_lots = (Lot *)realloc(heap->lots, heap->capacity * sizeof(Lot));
    if (new_lots == NULL) {
        // Se realloc fallisce, stampa un errore o gestisci il fallimento
        printf("Errore: impossibile allocare memoria per il MinHeap.\n");
        return;
    }
    heap->lots = new_lots;
}

// Inserisci un lotto in un min-heap (heapify)
void insert_lot(MinHeap_lots *heap, int quantity, int expiration) {

    if (heap->size == heap->capacity) {
        resize_minheap(heap);  // Ridimensiona il min-heap se pieno
    }
    heap->lots[heap->size].quantity = quantity;
    heap->lots[heap->size].expiration = expiration;
    int i = heap->size;
    heap->size++;

    // Heapify verso l'alto
    while (i > 0 && heap->lots[i].expiration < heap->lots[(i - 1) / 2].expiration) {
        Lot temp = heap->lots[i];
        heap->lots[i] = heap->lots[(i - 1) / 2];
        heap->lots[(i - 1) / 2] = temp;
        i = (i - 1) / 2;
    }
}

// Funzione heapify verso il basso per rimuovere un ingrediente
void remove_ingredient_lot(MinHeap_lots *heap) {
    // Sostituisci il lotto in cima con l'ultimo e riduci la dimensione
    heap->lots[0] = heap->lots[heap->size - 1];
    heap->size--;

    // Heapify verso il basso per ripristinare la proprietà del min-heap (solo se heap size > 0)
    int j = 0;
    while (j < heap->size) {
        int left = 2 * j + 1;
        int right = 2 * j + 2;
        int smallest = j;

        if (left < heap->size && heap->lots[left].expiration < heap->lots[smallest].expiration) {
            smallest = left;
        }
        if (right < heap->size && heap->lots[right].expiration < heap->lots[smallest].expiration) {
            smallest = right;
        }

        if (smallest != j) {
            Lot temp = heap->lots[j];
            heap->lots[j] = heap->lots[smallest];
            heap->lots[smallest] = temp;
            j = smallest;
        } else {
            break;
        }
    }
}

// Funzione per cercare un ingrediente all'interno della hash table ingredientTable
unsigned int search_ingredient(const char *name) {
    unsigned int hash1 = fnv1a_hash(name) % INGREDIENT_TABLE_SIZE;  // Prima funzione hash (indice iniziale)
    unsigned int step = hash1 % (INGREDIENT_TABLE_SIZE - 1) + 1;    // Passo incrementale
    unsigned int index = hash1;  // Indice iniziale
    unsigned int i = 0;          // Contatore di tentativi

    // Ciclo fino a trovare un bucket vuoto o l'ingrediente cercato
    while (ingredientTable[index] != NULL) {
        if (strcmp(ingredientTable[index]->name, name) == 0) {
            return index;  // Ingrediente trovato
        }
        if (i >= MAX_INGREDIENT_RETRIES) {
            return INGREDIENT_TABLE_SIZE;  // Troppi tentativi
        }
        i++;
        index = (hash1 + i * step) % INGREDIENT_TABLE_SIZE;
    }

    return INGREDIENT_TABLE_SIZE;  // Bucket vuoto, l'ingrediente non c'è nella hash table degli ingredienti

}

// Funzione per aggiungere un ingrediente e il suo lotto
void add_ingredient(const char *name, int quantity, int expiration) {

    unsigned int hash1 = fnv1a_hash(name) % INGREDIENT_TABLE_SIZE;  // Prima funzione hash (indice iniziale)
    unsigned int step = hash1 % (INGREDIENT_TABLE_SIZE - 1) + 1;    // Passo incrementale
    unsigned int index = hash1;  // Indice iniziale
    unsigned int i = 0;          // Contatore di tentativi

    // Finchè non trovi un bucket vuoto o lo stesso ingrediente vai al prossimo bucket
    while (ingredientTable[index] != NULL && strcmp(ingredientTable[index]->name, name) != 0) {
        if (i >= MAX_INGREDIENT_RETRIES) {       //se il numero di tentativi il massimo ci rinunciamo
            // La tabella è piena
            return;
        }
        i++;
        index = (hash1 + i * step) % INGREDIENT_TABLE_SIZE;
    }

    // Se l'ingrediente non esiste, crealo
    if (ingredientTable[index] == NULL) {
        Ingredient *newIngredient = malloc(sizeof(Ingredient));
        newIngredient->name = malloc(sizeof(char)*(strlen(name)) + 1);
        strcpy(newIngredient->name, name);
        newIngredient->heap = create_minheap_lots(10);  // Capacità iniziale del heap
        newIngredient->total_quantity = quantity;
        ingredientTable[index] = newIngredient;

        insert_lot(ingredientTable[index]->heap, quantity, expiration);
    }
    else {
        ingredientTable[index]->total_quantity += quantity;
        MinHeap_lots *heap = ingredientTable[index]->heap;
        bool found = false;
        for (i = 0; i < heap->size && heap->lots[i].expiration >= expiration; ++i) {
            if(heap->lots[i].expiration == expiration) {
                heap->lots[i].quantity += quantity;
                found = true;
                break;
            }
        }
        if (!found) {
            insert_lot(heap, quantity, expiration);
        }
    }

}

// Rimuovi i lotti scaduti
void remove_expired_lots(int current_tick) {
    for (int i = 0; i < INGREDIENT_TABLE_SIZE; i++) {
        if (ingredientTable[i] != NULL) {
            MinHeap_lots *heap = ingredientTable[i]->heap;
            // Rimuovi lotti scaduti dal min-heap
            while (heap->size > 0 && heap->lots[0].expiration <= current_tick) {
                ingredientTable[i]->total_quantity -= heap->lots[0].quantity;
                remove_ingredient_lot(heap);
            }
        }
    }
}

// ****____****____****____****____**** GESTIONE RICETTE ****____****____****____****____****

int count_ingredients_recipe(char *str) {
    int count = 1;
    while(*str != '\0' && *str != '\n') {
        if(*str == ' ') {
            count++;
        }
        str++;
    }
    return count/2;
}

// Crea una ricetta con nome e ingredienti
void add_recipe(const char *recipe_name, char *rest_of_line) {
    unsigned int hash1 = fnv1a_hash(recipe_name) % RECIPE_TABLE_SIZE;  // Prima funzione hash (indice iniziale)
    unsigned int step = hash1 % (RECIPE_TABLE_SIZE - 1) + 1;           // Passo incrementale
    unsigned int index = hash1;  // Indice iniziale
    unsigned int i = 0;          // Contatore di tentativi

    while (recipeTable[index] != NULL && strcmp(recipeTable[index]->name, recipe_name) != 0) {
        if (i >= MAX_RECIPE_RETRIES) {
            // La tabella è piena
            return;
        }
        i++;
        index = (hash1 + i * step) % RECIPE_TABLE_SIZE;
    }

    // Se la ricetta non esiste, creala
    if (recipeTable[index] == NULL) {
        // Ottieni la lista degli ingredienti
        //IngredientNode *ingredient_list = recipe_ingredient_list(rest_of_line);
        char *per_conta = rest_of_line;
        int count = count_ingredients_recipe(per_conta);
        IngredientNode *ingredient_array = malloc(count * sizeof(IngredientNode));

        // Processa la stringa "rest_of_line" per ottenere ingredienti e quantità
        int total_weight = 0;
        char *ingredient_name = strtok(rest_of_line, " ");
        for(i = 0; i < count; i++) {
            // Crea un nuovo nodo per l'ingrediente
            unsigned int hash = search_ingredient(ingredient_name);
            if(hash == INGREDIENT_TABLE_SIZE) {
                ingredient_array[i].name = malloc(sizeof(char)*(strlen(ingredient_name)) + 1);
                strcpy(ingredient_array[i].name, ingredient_name);
            }
            else {
                ingredient_array[i].name = NULL;
            }
            ingredient_array[i].quantity = string_to_int(strtok(NULL, " "));  // Leggi la quantità
            ingredient_array[i].hash = hash;
            total_weight += ingredient_array[i].quantity;

            // Leggi il prossimo ingrediente
            ingredient_name = strtok(NULL, " ");
        }

        Recipe *newRecipe = (Recipe*) malloc(sizeof(Recipe));
        newRecipe->name = malloc(sizeof(char)*(strlen(recipe_name)) + 1);
        strcpy(newRecipe->name, recipe_name);
        newRecipe->weight = total_weight;
        newRecipe->ingredients = ingredient_array;
        newRecipe->last_quantity_failed = 0;
        newRecipe->last_tick_check = 0;
        newRecipe->ingredients_size = count;
        recipeTable[index] = newRecipe;
        printf("aggiunta\n");
    } else {
        printf("ignorato\n");
    }
}

// Funzione per cercare una ricetta, ritorna l'indice della ricetta
unsigned int search_recipe(const char *recipe_name) {
    unsigned int hash1 = fnv1a_hash(recipe_name) % RECIPE_TABLE_SIZE;  // Prima funzione hash (indice iniziale)
    unsigned int step = hash1 % (RECIPE_TABLE_SIZE - 1) + 1;           // Passo incrementale
    unsigned int index = hash1;  // Indice iniziale
    unsigned int i = 0;          // Contatore di tentativi

    // Cerca la ricetta nella hash table
    while (recipeTable[index] != NULL && strcmp(recipeTable[index]->name, recipe_name) != 0) {
        if (i >= MAX_RECIPE_RETRIES) {
            return RECIPE_TABLE_SIZE;
        }
        i++;
        index = (hash1 + i * step) % RECIPE_TABLE_SIZE;
    }

    // Verifica se la ricetta è stata trovata
    Recipe *recipe = recipeTable[index];
    if (recipe == NULL) { // Ricetta non trovata
        return RECIPE_TABLE_SIZE;
    }

    return index;
}

// Funzione per rimuovere una ricetta
void remove_recipe(const char *recipe_name, MinHeap_orders *ready_orders_heap) {
    unsigned int index = search_recipe(recipe_name);
    if (index == RECIPE_TABLE_SIZE) {
        printf("non presente\n");
        return;
    }
    Recipe *recipe = recipeTable[index];

    // Verifica se ci sono ordini in attesa per questa ricetta
    OrderNode *current_order = pending_orders_list.head;
    while (current_order != NULL) {
        if (current_order->recipe == recipe) {
            printf("ordini in sospeso\n");
            return;
        }
        current_order = current_order->next;
    }

    // Verifica se ci sono ordini pronti per questa ricetta nel min-heap ready_orders_heap
    if(ready_orders_heap != NULL){
        for (int i = 0; i < ready_orders_heap->size; i++) {
            if (ready_orders_heap->orders[i]->recipe == recipe) {
                printf("ordini in sospeso\n");
                return;
            }
        }
    }

    // Non ci sono ordini in sospeso, possiamo rimuovere la ricetta
    // Libera array degli ingredienti
    IngredientNode *ingredient_array = recipe->ingredients;
    for (int i = 0; i < recipe->ingredients_size; ++i) {
        if(ingredient_array[i].name != NULL){
            free(ingredient_array[i].name);
        }
    }
    free(ingredient_array);

    // Libera la ricetta e rimuovila dalla hash table
    free(recipe->name);
    free(recipe);
    recipeTable[index] = NULL;  // Elimina la ricetta dalla hash table
    printf("rimossa\n");
}

// ****____****____****____****____**** GESTIONE ORDINI DA CARICARE ****____****____****____****____****

// Funzione per ridimensionare il Min-Heap
void resize_minheap_orders(MinHeap_orders *heap) {
    if(heap != NULL) {
        if (heap->orders == NULL) return;
        heap->capacity *= 2;  // Raddoppia la capacità
        OrderNode **new_orders = (OrderNode **)realloc(heap->orders, heap->capacity * sizeof(OrderNode *));
        if (new_orders == NULL) {
            // Se realloc fallisce, stampa un errore o gestisci il fallimento
            printf("Errore: impossibile allocare memoria per il MinHeap.\n");
            return;
        }
        heap->orders = new_orders;
    }
}

// Inserisci un lotto in un min-heap (heapify)
void insert_ready_order_in_minheap(MinHeap_orders *heap, OrderNode *order) {
    if(heap != NULL){
        if (heap->size == heap->capacity) {
            resize_minheap_orders(heap);  // Ridimensiona il min-heap se pieno
        }
        heap->orders[heap->size] = order;
        int i = heap->size;
        heap->size++;
        // Heapify verso l'alto
        while (i > 0 && heap->orders[i]->tick < heap->orders[(i - 1) / 2]->tick) {
            OrderNode *temp = heap->orders[i];
            heap->orders[i] = heap->orders[(i - 1) / 2];
            heap->orders[(i - 1) / 2] = temp;
            i = (i - 1) / 2;
        }
    }
}

void heapify_down(MinHeap_orders *heap, int index) {
    int smallest = index;
    int left_child = 2 * index + 1;
    int right_child = 2 * index + 2;

    if(heap != NULL){
        if (left_child < heap->size && heap->orders[left_child]->tick < heap->orders[smallest]->tick) {
            smallest = left_child;
        }
        if (right_child < heap->size && heap->orders[right_child]->tick < heap->orders[smallest]->tick) {
            smallest = right_child;
        }

        if (smallest != index) {
            OrderNode *temp = heap->orders[index];
            heap->orders[index] = heap->orders[smallest];
            heap->orders[smallest] = temp;
            heapify_down(heap, smallest);
        }
    }
}

// Sceglie gli ordini da caricare e li sposta dal min_heap alla lista orders_to_load
void choose_orders_to_load(int courier_capacity, MinHeap_orders *ready_orders_heap) {
    int remaining_capacity = courier_capacity;

    if(ready_orders_heap != NULL){
        while (ready_orders_heap->size > 0) {
        OrderNode *current_order = ready_orders_heap->orders[0]; // Radice del min-heap
        int current_order_weight = current_order->quantity * current_order->recipe->weight;
        // Controlla se l'ordine corrente può essere caricato
        if (current_order_weight <= remaining_capacity) {
            // Riduci la capacità rimanente
            remaining_capacity -= current_order_weight;

            // Rimuovi il nodo minimo dal heap
            ready_orders_heap->orders[0] = ready_orders_heap->orders[ready_orders_heap->size - 1];
            ready_orders_heap->size--;
            heapify_down(ready_orders_heap, 0);

            // Aggiungi l'ordine alla coda della lista orders_to_load
            current_order->next = NULL;
            current_order->prev = orders_to_load.tail;

            if (orders_to_load.tail != NULL) {
                (orders_to_load.tail)->next = current_order;
            } else {
                // Se la lista era vuota, aggiorna anche la testa
                orders_to_load.head = current_order;
            }

            // Aggiorna la coda
            orders_to_load.tail = current_order;

        } else {
            // L'ordine corrente eccede la capacità, interrompi
            break;
        }
    }
    }
}

// Funzione quick-sort per ordinare la lista orders_to_load
OrderNode *partition(OrderNode *low, OrderNode *high) {
    int pivot_weight = high->quantity * high->recipe->weight; // Scelgo il peso come pivot
    int pivot_tick = high->tick;          // Uso il tick per stabilità

    OrderNode *i = low->prev; // Indice degli elementi più grandi

    for (OrderNode *j = low; j != high; j = j->next) {
        // Confronto per peso, poi per tick
        int j_weight = j->quantity * j->recipe->weight;
        if (j_weight > pivot_weight || (j_weight == pivot_weight && j->tick < pivot_tick)) {
            // Incrementa l'indice di elementi più grandi
            i = (i == NULL) ? low : i->next;

            // Scambia i nodi i e j
            Recipe *temp_recipe = i->recipe;
            int temp_tick = i->tick;
            int temp_quantity = i->quantity;

            i->recipe = j->recipe;
            i->tick = j->tick;
            i->quantity = j->quantity;

            j->recipe = temp_recipe;
            j->tick = temp_tick;
            j->quantity = temp_quantity;
        }
    }

    // Scambia i+1 e il pivot
    if (i == NULL) {
        i = low;
    } else {
        i = i->next;
    }

    Recipe *temp_recipe = i->recipe;
    int temp_tick = i->tick;
    int temp_quantity = i->quantity;

    i->recipe = high->recipe;
    i->tick = high->tick;
    i->quantity = high->quantity;

    high->recipe = temp_recipe;
    high->tick = temp_tick;
    high->quantity = temp_quantity;

    return i;
}

void quicksort(OrderNode *low, OrderNode *high) {
    if (low && high != NULL && low != high && low != high->next) {
        OrderNode *pivot = partition(low, high);

        quicksort(low, pivot->prev);
        quicksort(pivot->next, high);
    }
}

// Funzione per "caricare" il corriere
void load_courier(int courier_capacity, MinHeap_orders *ready_orders_heap) {
    // scelgo che ordini caricare
    choose_orders_to_load(courier_capacity, ready_orders_heap);
    // ordino la lista orders_to_load
    quicksort(orders_to_load.head, orders_to_load.tail);

    int total_elements = 0;
    OrderNode *current = orders_to_load.head;

    while (current != NULL) {
        // Stampa il nome della ricetta
        printf("%d %s %d\n", current->tick, current->recipe->name, current->quantity);

        // Salva il nodo corrente da eliminare
        OrderNode *to_delete = current;

        // Passa al prossimo nodo
        current = current->next;

        // Elimina il nodo corrente
        free(to_delete);

        // Aggiorna la testa della lista
        orders_to_load.head = current;
        total_elements++;
    }

    // La lista è ora vuota, resetta la coda
    orders_to_load.tail = NULL;
    if(total_elements == 0){
        printf("camioncino vuoto\n");
    }
}

// ****____****____****____****____**** GESTIONE ORDINI ****____****____****____****____****

// Sposta ordini da lista pending orders a minheap ordini pronti
void move_order_to_minheap(OrderNode *order, MinHeap_orders *ready_orders_heap){
    if (order == NULL) {
        //Errore: ordine nullo
        return;
    }
    // Rimuovi il nodo dalla lista pending_orders_list
    if (order == pending_orders_list.head) {  // Nodo in testa
        pending_orders_list.head = order->next; // Sposta la testa al prossimo ordine
        if (pending_orders_list.head != NULL) { // Se l'ordine corrente esiste
            pending_orders_list.head->prev = NULL; // imposta il precedente a NULL perché sei in testa
        } else {
            pending_orders_list.tail = NULL;  // La lista diventa vuota
        }
    } else if (order == pending_orders_list.tail) {  // Nodo in coda
        pending_orders_list.tail = order->prev;
        if (pending_orders_list.tail != NULL) { // Se esiste un nodo in coda
            pending_orders_list.tail->next = NULL;  // Imposta il prossimo nodo della nuova coda a NULL.
        } else {
            pending_orders_list.head = NULL;  // La lista diventa vuota
        }
    } else {  // Nodo in posizione intermedia
        order->prev->next = order->next;
        order->next->prev = order->prev;
    }
    order->prev = order->next = NULL;
    insert_ready_order_in_minheap(ready_orders_heap, order);

}

// Esegue un ordine se è stato verificato con successo
void make_order(OrderNode *order, MinHeap_orders *ready_orders_heap, Recipe *recipe) {

    // Sottrai gli ingredienti dai lotti
    IngredientNode *ingredient_array = recipe->ingredients;
    for(int i=0; i<recipe->ingredients_size; i++) {
        int total_required = ingredient_array[i].quantity * order->quantity;
        Ingredient *ingredient = ingredientTable[ingredient_array[i].hash];

        if (ingredient == NULL) {
            return;
        }

        // Sottrai la quantità dal totale disponibile dell'ingrediente
        ingredient->total_quantity -= total_required;

        // Sottrai la quantità dai lotti
        MinHeap_lots *heap = ingredient->heap;
        for (int j = 0; j < heap->size && total_required > 0; j++) {
            if (heap->lots[j].quantity <= total_required) {
                total_required -= heap->lots[j].quantity;
                heap->lots[j].quantity = 0;  // Elimina il lotto
            } else {
                heap->lots[j].quantity -= total_required;
                total_required = 0;
            }

            // Rimuovi i lotti esauriti (se la quantità è 0)
            if (heap->lots[j].quantity == 0) {
                remove_ingredient_lot(heap);
                j--;
            }
        }
    }

    // Ora l'ordine è pronto
    move_order_to_minheap(order, ready_orders_heap);
}

// Controlla se l'ordine può essere eseguito
void check_order(OrderNode *current_order, MinHeap_orders *ready_orders_heap, int tick) {

    Recipe *recipe = current_order->recipe;
    // Verifica se ci sono abbastanza ingredienti per l'ordine
    IngredientNode *ingredient_array = recipe->ingredients;

    for(int i=0; i<recipe->ingredients_size; i++) {
        int total_required = ingredient_array[i].quantity * current_order->quantity;
        int total_available = 0;
        unsigned int index = ingredient_array[i].hash;

        if(index == INGREDIENT_TABLE_SIZE) {
            // Cerca l'ingrediente nella hash table
            index = search_ingredient(ingredient_array[i].name);

            if(index == INGREDIENT_TABLE_SIZE) {
                return;
            }
            ingredient_array[i].hash = index;      //aggiorno hash di ingredient_node
            free(ingredient_array[i].name);
            ingredient_array[i].name = NULL;
        }

        Ingredient *ingredient = ingredientTable[index];

        if (ingredient == NULL) {
            return;
        }

        // Somma i lotti disponibili nel min-heap dell'ingrediente
        int j = 0;
        while (j < ingredient->heap->size && total_available < total_required) {
            if (ingredient->heap->lots[j].expiration > tick) {
                total_available += ingredient->heap->lots[j].quantity;
                j++;
            } else {
                ingredient->total_quantity -= ingredient->heap->lots[j].quantity;
                remove_ingredient_lot(ingredient->heap);
            }
        }

        // Se la quantità richiesta non è disponibile, interrompi il controllo
        if (total_available < total_required) {
            recipe->last_tick_check = tick;                          //aggiorno il tick dell'ultimo fallimento
            recipe->last_quantity_failed = current_order->quantity;  //aggiorno la quantità dell'ultimo fallimento
            return;
        }
    }

    // Se l'ordine può essere eseguito, chiama make_order
    make_order(current_order, ready_orders_heap, recipe);
}

// Inserisci un nuovo ordine in coda, controlla se esiste la ricetta e assegna il peso
void add_order(const char *recipe_name, int quantity, int tick, MinHeap_orders *ready_orders_heap) {
    unsigned int index = search_recipe(recipe_name);
    if (index == RECIPE_TABLE_SIZE) {
        printf("rifiutato\n");
        return;
    }
    Recipe *recipe = recipeTable[index];

    OrderNode *newOrder = malloc(sizeof(OrderNode));
    newOrder->recipe = recipe;
    newOrder->quantity = quantity;
    newOrder->tick = tick;
    newOrder->next = NULL;

    if (pending_orders_list.tail == NULL) {
        // La lista è vuota
        newOrder->prev = NULL;
        pending_orders_list.head = newOrder;
        pending_orders_list.tail = newOrder;
    } else {
        // Aggiungi in coda
        newOrder->prev = pending_orders_list.tail;
        pending_orders_list.tail->next = newOrder;
        pending_orders_list.tail = newOrder;
    }
    printf("accettato\n");

    // Se la ricetta con quantità minore o uguale è già fallita con lo scorso rifornimento non eseguo il check_order
    if(newOrder->recipe->last_tick_check >= last_supply_tick && newOrder->quantity >= newOrder->recipe->last_quantity_failed) {
        return;
    }
    check_order(newOrder, ready_orders_heap, tick);
}

// Controlla la lista degli ordini in attesa e verifica se possono essere eseguiti
void check_orders(MinHeap_orders *ready_orders_heap, int tick) {
    OrderNode *current_order = pending_orders_list.head;
    //int i = 0;
    // Scorri la lista degli ordini in attesa
    while (current_order != NULL) {
        OrderNode *next_order = current_order->next;

        //non controllare la ricetta se la quantità dell'ordine è maggiore o uguale all'ultima quantità fallita e il tick nella ricetta è quello corrente
        if(current_order->recipe->last_quantity_failed > current_order->quantity || tick != current_order->recipe->last_tick_check) {

            bool order_can_be_made = true;
            // Verifica se ci sono abbastanza ingredienti per l'ordine
            IngredientNode *ingredient_array = current_order->recipe->ingredients;
            for(int i=0; i<current_order->recipe->ingredients_size; i++) {
                int total_required = ingredient_array[i].quantity * current_order->quantity;
                unsigned int index = ingredient_array[i].hash;

                if(index == INGREDIENT_TABLE_SIZE) {
                    // Cerca l'ingrediente nella hash table
                    index = search_ingredient(ingredient_array[i].name);
                    if(index == INGREDIENT_TABLE_SIZE) {
                        break; // l'ingrediente non esiste nella hash table ingredienti
                    }
                    ingredient_array[i].hash = index;      //aggiorno hash di ingredient_node
                    free(ingredient_array[i].name);
                    ingredient_array[i].name = NULL;
                }

                Ingredient *ingredient = ingredientTable[index];

                if (ingredient == NULL) {
                    order_can_be_made = false;
                    break;
                }

                if(ingredient->total_quantity < total_required) {
                    order_can_be_made = false;
                    break;
                }
            }
            // Se l'ordine può essere eseguito, chiama make_order
            if (order_can_be_made) {
                make_order(current_order, ready_orders_heap, current_order->recipe);
            }
            else {
                current_order->recipe->last_tick_check = tick;
                current_order->recipe->last_quantity_failed = current_order->quantity;
            }
        }
        current_order = next_order;
    }
}

void free_all_memory() {

    int i = 0;
    for (i = 0; i < INGREDIENT_TABLE_SIZE; i++) {
        if(ingredientTable[i] != NULL) {
            free(ingredientTable[i]->heap->lots);
            free(ingredientTable[i]->heap);
            free(ingredientTable[i]->name);
            free(ingredientTable[i]);
        }
    }

    for (i = 0; i < RECIPE_TABLE_SIZE; i++) {
        if(recipeTable[i] != NULL) {
            IngredientNode *ingredient_array = recipeTable[i]->ingredients;
            for(int j=0; j<recipeTable[i]->ingredients_size; j++) {
                if(ingredient_array[j].name!=NULL) {
                    free(ingredient_array[j].name);
                }
            }
            free(recipeTable[i]->ingredients);
            free(recipeTable[i]->name);
            free(recipeTable[i]);
        }
    }
    // Ora liberiamo la pending order list
    while(pending_orders_list.head != NULL) {
        // se la testa è uguale alla coda
        if(pending_orders_list.tail == pending_orders_list.head){
            free(pending_orders_list.tail);
            pending_orders_list.head = NULL;
            pending_orders_list.tail = NULL;
        }
        else {
            OrderNode *temp = pending_orders_list.tail;
            pending_orders_list.tail = pending_orders_list.tail->prev;
            free(temp);
            temp = NULL;
        }
    }
    free(orders_to_load.head);
    free(orders_to_load.tail);
}

int main(){
    char *line = NULL;  // Buffer dinamico per la riga
    size_t len = 0;
    int courier_frequency, courier_capacity;
    int tick = 0;
    MinHeap_orders *ready_orders_heap = NULL;
    ready_orders_heap = create_minheap_orders(25);

    //printf("Hello World\n");
    FILE* file = stdin;
    if (file == NULL) {
        printf("Errore: impossibile aprire il file.\n");
        return 1;
    }

    // Leggi la prima riga dal file
    if (getline(&line, &len, file) == -1) {
        printf("Errore durante la lettura della riga dal file\n");
        fclose(file);
        free(line);  // Rilascia il buffer
        return 1;
    }

    // Estrai i due numeri dalla stringa letta
    if (sscanf(line, "%d %d", &courier_frequency, &courier_capacity) != 2) {
        printf("Errore durante la lettura dei valori dalla prima riga\n");
        fclose(file);
        return 1;
    }

    // Leggi il file riga per riga
    while (getline(&line, &len, file) != -1) {

        // Verifichiamo se è l'ora dello sbusto
        if(tick % courier_frequency == 0 && tick != 0){
            // Esegui la funzione load_courier
            load_courier(courier_capacity, ready_orders_heap);
        }

        // Ottieni il comando (es. "aggiungi_ricetta", "rimuovi_ricetta", "rifornimento", "ordine")
        char *command = strtok(line, " ");

        if (strcmp(command, "aggiungi_ricetta") == 0) {

            // Leggi il nome della ricetta
            char *recipe_name = strtok(NULL, " ");

            // Leggi il resto della linea (ingredienti e quantità)
            char *rest_of_line = strtok(NULL, "\n");

            // Aggiungi la ricetta con il nome e il resto della linea
            add_recipe(recipe_name, rest_of_line);

        } else if (strcmp(command, "rimuovi_ricetta") == 0) {
            // Leggi il nome della ricetta da rimuovere
            char *recipe_name = strtok(NULL, "\n");
            remove_recipe(recipe_name, ready_orders_heap);

        } else if (strcmp(command, "rifornimento") == 0) {
            // Rimuovi i lotti scaduti
            remove_expired_lots(tick);
            // Leggi gli ingredienti e le quantità/scadenze
            last_supply_tick = tick;
            char *ingredient_name = strtok(NULL, " ");
            while (ingredient_name != NULL) {
                // Leggi quantità e scadenza
                int quantity = string_to_int(strtok(NULL, " "));
                int expiration = string_to_int(strtok(NULL, " "));

                // Aggiungi l'ingrediente con la quantità e la scadenza solo se la scadenza non è immediata e la quantità è >0
                if (expiration > tick && quantity > 0) {
                    add_ingredient(ingredient_name, quantity, expiration);
                }

                // Leggi il prossimo ingrediente
                ingredient_name = strtok(NULL, " ");
            }
            printf("rifornito\n");
            check_orders(ready_orders_heap, tick);

        } else if (strcmp(command, "ordine") == 0) {
            // Leggi il nome della ricetta e la quantità ordinata
            char *recipe_name = strtok(NULL, " ");
            int quantity = string_to_int(strtok(NULL, " "));
            add_order(recipe_name, quantity, tick, ready_orders_heap);

        } else {
            printf("#Comando non riconosciuto: %s\n", command);
        }
        tick++;
    }

    // Se il prossimo istante dopo la fine del file arriva il corriere si sbusta
    if(tick % courier_frequency == 0 && tick != 0){
        load_courier(courier_capacity, ready_orders_heap);
    }

    fclose(file);
    free(line);
    // Libero ready_orders_heap
    if(ready_orders_heap != NULL) {
        for(int i = 0; i < ready_orders_heap->size; i++) {
            if(ready_orders_heap->orders[i] != NULL){
                free(ready_orders_heap->orders[i]);
                ready_orders_heap->orders[i] = NULL;
            }
        }
        free(ready_orders_heap->orders);
        free(ready_orders_heap);
    }
    free_all_memory();
    return 0;
 }

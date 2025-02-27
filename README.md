![C](https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white)
[![CLion](https://img.shields.io/badge/CLion-black?style=for-the-badge&logo=clion&logoColor=white)](https://www.jetbrains.com/clion/)

<h2 align="center">Progetto Finale Algoritmi e Principi dell'Informatica (API) 2024 - Politecnico di Milano</h2>

L'obiettivo del progetto è realizzare uno script che, dato un file di testo in ingresso in standard input, produca in uscita, nel minor tempo possibile e con il minimo utilizzo di memoria, un output corretto che rispetti la specifica.

Quest'anno il progetto chiede di progettare un sistema che simuli il funzioniamento di una pasticceria. L’intera simulazione avviene a tempo discreto e, a valle dell’esecuzione di ogni comando ricevuto in ingresso, trascorre un istante di tempo. 
Nella simulazione si dovranno considerare i seguenti elementi:
* Gli ingredienti dei dolci, ognuno identificato dal suo nome, costituito da una sequenza di caratteri.
* L’insieme delle ricette offerte dalla pasticceria, identificate anch’esse da un nome. Ogni ricetta utilizza diverse quantità di ciascun ingrediente necessario (indicate da un numero intero, in grammi).
* Il magazzino degli ingredienti della pasticceria, che stocca ogni ingrediente utilizzato. Il magazzino viene rifornito da nuovi lotti di ingredienti, in base a una pianificazione stabilita dal fornitore. Ogni lotto è caratterizzato da una quantità (sempre in grammi) e da una data di scadenza, indicata dal numero dell’istante di tempo a partire dal quale il lotto è scaduto.
* I clienti della pasticceria effettuano ordini di uno o più dolci e la pasticceria procede subito a preparare i dolciumi ordinati. Si può assumere che la preparazione di un numero arbitrario di dolci avvenga in un singolo istante della simulazione. Gli ingredienti necessari per ogni preparazione vengono prelevati dal magazzino privilegiando sempre i lotti con la scadenza più prossima. Se non sono disponibili ingredienti a sufficienza da consentire la preparazione per intero di un ordine, esso viene messo in attesa. È possibile avere un numero arbitrario di ordini in attesa. La pasticceria procede a preparare eventuali ordini successivi. Ad ogni rifornimento, la pasticceria valuta se è possibile, con gli ingredienti ricevuti, preparare ordini attualmente in attesa. Se questo è il caso, li prepara nello stesso istante di tempo. Gli ordini in attesa vengono smaltiti in ordine cronologico di arrivo dell’ordine.
* Periodicamente, il corriere si reca dalla pasticceria a ritirare gli ordini pronti. All’arrivo del corriere, gli ordini da caricare vengono scelti in ordine cronologico di arrivo. Il processo si ferma appena viene incontrato un ordine che supera la capienza rimasta (in grammi) sul camioncino. Si assume che il peso di ogni dolce preparato sia uguale alla somma delle quantità in grammi di ciascun ingrediente. Ogni ordine viene sempre caricato nella sua interezza. Scelti gli ordini, la pasticceria procede a caricarli in ordine di peso decrescente. A parità di peso, gli ordini vanno caricati in ordine cronologico di arrivo.

Il file testuale in ingresso inizia con una riga contenente due interi: la periodicità del corriere e la sua
capienza.

I comandi in ingresso sono di 4 tipi:
* aggiungi_ricetta ⟨nome_ricetta⟩ ⟨nome_ingrediente⟩ ⟨quantità⟩ ... : Aggiunge una ricetta al catalogo, esempio: aggiungi_ricetta meringhe_della_prozia zucchero 100 albumi 100
* rimuovi_ricetta ⟨nome_ricetta⟩ : Rimuove una ricetta dal catalogo. Non ha effetto se la ricetta non è presente, oppure ci sono ordini relativi ad essa non ancora spediti.
* rifornimento ⟨nome_ingrediente⟩ ⟨quantità⟩ ⟨scadenza⟩ ... : La pasticceria viene rifornita di un insieme di lotti, uno per ingrediente. Il numero di lotti è arbitrario. Esempio: rifornimento zucchero 200 150 farina 1000 220
* ordine ⟨nome_ricetta⟩ ⟨numero_elementi_ordinati⟩ : Effettua un ordine di ⟨numero_elementi_ordinati⟩ dolci con ricetta ⟨nome_ricetta⟩. Stampa attesa come risposta: accettato oppure rifiutato se non esiste nessuna ricetta col nome specificato.



Le prestazioni raggiunte dal mio script sul verificatore sono le seguenti: esecuzione in 1,99s e 13,7 MiB di memoria utilizzati, risuiltati eligibili per una valutazione di 30 e Lode, ma, data la struttura "particolare" del verificatore, il voto registrato è 30.

## Implementazione

Il programma è stato svilupppato in C. 

Per l'implementazione sono state utilizzate diverse strutture di dati: 
* Hash table per gli ingredienti e per le ricette
* Ogni ingrediente ha un min-heap di lotti ordinati in modo di avere in cima al mucchio il lotto con la scadenza più vicina
* Ogni ricetta ha una lista semplice di ingredienti per memorizzare nome e quantità necessaria di ciascuno
* Lista doppiamente collegata per gli ordini
* Min-Heap per gli ordini pronti in modo da avere in cima l'ordine con il tempo di arrivo (che non è il tempo di preparazione) più basso
* Lista per gli ordini pronti da caricare, che verrà ordinata tramite un algoritmo quicksort 

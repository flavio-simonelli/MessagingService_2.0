# MessagingService_2.0

Server: il server adesso inizializza un file credenziali senza mutex e senza struttra dati adeguata, inizializza la socket correttamente, inizializza i segnali e inizializza i trheads
- aggiunta struttura dati hash table per utenti e completata inizializzazione credenziali
Bisogna scrivere nel server le funzioni per inserire controllare o eliminare un utente
Tutto fatto l'autenticazione sembra funzionare correttamente, adesso bisogna fare le viarie chat 
Ho inserito una lista di strutture dati per contenere nome file chat e mutex per la scrittura, ho aggiunto le funziioni per inizilizzare la struttra dati lista, aggiunger rimuovere e cercare un nuodo nella lista e adesso stavo scrivendo la funzione scrivi un messsaggio.
Bisogna completare questa e inserire tutte le firme e aggiustare tutte le funzioni dei messaggi.
Aggiungere una funzione facoltativa che ti dice quali chat sono aperte con quali utenti (bisogna aggiunger al nodo file chat i due nomi stringhe per ogni partecipante alla chat)
Aggiungere una funzione per eliminare un messaggio
Aggiungere una funzione per stampare a schermo tutti i messaggi di una chat
Magari aggiungere anche la possibilità di creareu gruppo
Poi scrivere le funzioni del client
Poi trasformasre il client eseguibile per windows

Client: inizializza la socket corretamente e inizializza la crypto scambiando anche le chiavi utili per l'invio dei messaggi




MI MANCA:
1) inserire un nuovo messaggio nella chat (il messaggio divide i vari campi tramite lo \n) (la funzione esiste già bisogna solo fare la logica applicativa)
\/2) eliminare un messaggio (bisogna scrivere pure initchat perchè non va bene quella del file)
4) eliminare utente (bi)
\/5) stampare tutta la chat
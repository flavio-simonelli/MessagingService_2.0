# MessagingService_2.0

Server: il server adesso inizializza un file credenziali senza mutex e senza struttra dati adeguata, inizializza la socket correttamente, inizializza i segnali e inizializza i trheads
- aggiunta struttura dati hash table per utenti e completata inizializzazione credenziali

Client: inizializza la socket corretamente e inizializza la crypto scambiando anche le chiavi utili per l'invio dei messaggi

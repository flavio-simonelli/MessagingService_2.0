#include "server.h"


int serverSocket; // descrittore della socket lato server
pthread_mutex_t writecredential;
pthread_mutex_t writechat;
//database
HashNode* utenteTable;
HashNode* chatTable;



int main(int argc, char **argv){
    // inseriamo 2 parametri: indirizzo ip e porta del server
    // controllo inserimento dei 2 parametri
    if(argc != 3){
        fprintf(stderr,"Usare: %s [IP address] (if IP=null is the same of machine) [port]\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    //inizializzazione della gestione dei segnali
    if(initSignal() != 0){
        fprintf(stderr,"errore nell'inizializzazione dei segnali\n");
        exit(EXIT_FAILURE);
    }

    //inizializzazione della socket
    if(initSocket(argv[1], argv[2]) != 0){
        fprintf(stderr,"errore nell'inizializzazione della socket\n");
        exit(EXIT_FAILURE);
    }

    //inizializzazione della libreria di crypto
    if(initCrypto() != 0){
        fprintf(stderr,"Errore nell'inizializzazione della libreria crittografica\n");
        closeServer();
        exit(EXIT_FAILURE);
    }

    // inizializzazione del file credenziali e della struttura dati tableuser
    if(initDataBase() != 0){
        fprintf(stderr,"Errore: impossibile inizializzare i database\n");
        closeServer();
        exit(EXIT_FAILURE);
    }

    //accettazione di nuove connessioni con i client attraverso threads separati
    while(1){
        //mettiamo la socket in stato di accettazione
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int clientSocket = accept(serverSocket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (clientSocket < 0) {
            perror("Errore connessione alla socket");
            closeServer();
            exit(EXIT_FAILURE);
        }
        //creiamo un thread per ogni nuova connessione in ingresso
        pthread_t thread;
        if(pthread_create(&thread,NULL,mainThread,(void *)&clientSocket) != 0){
            perror("Errore creazione nuovo thread");
            close(clientSocket);
            closeServer();
            exit(EXIT_FAILURE);
        }
        printf("connesso un nuovo client! \n");
    }
    closeServer();
    return 0;
}


/* Funzione che chiude il server correttamente */
void signalclose(){
    closeServer();
    printf("\n Il server e' stato chiuso correttamente\n");
    exit(EXIT_SUCCESS);
}

/* Questa funzione serve a chiudere correttamente tutte le componenti del server*/
void closeServer(){
    close(serverSocket);
}

/* funzione che inizializza la maschera dei segnali da gestire */
int initSignal(){
    sigset_t mask;
    //inizializzazione della struttura per sigaction
    struct sigaction sa;
    sa.sa_handler = signalclose;
    sa.sa_flags = SA_RESTART;

    sigfillset(&mask); // aggiungiamo alla maschera tutti i segnali
    sigdelset(&mask, SIGINT); // togliamo dalla maschera SIGINT
    sigdelset(&mask, SIGPIPE); // togliamo dalla maschera SIGPIPE
    sigdelset(&mask, SIGTERM); // togliamo il segnale SIGTERM per terminare il server

    if(sigprocmask(SIG_SETMASK, &mask, NULL) != 0){
        perror("Errore nell'impostare la maschera dei segnali");
        return 1;
    }

    // configurazione sigaction per SIGTERM
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Errore durante la configurazione di SIGTERM");
        return 1;
    }
    // Configura sigaction per SIGINT
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Errore durante la configurazione di SIGINT");
        return 1;
    }

    return 0;
    /* adesso SIGTERM E SIGINT fanno la stessa cosa mentre SIGPIPE è ignorato implicitamente quidni comportamnento di default */

}


/* funzione per l'inizializzazione della socket, la funzione crea la socket ma non la mette in ascolto */
int initSocket (char *ipAddress, char *portstring){
    int functret; // intero utilizzato per controllare il valore di ritorno delle funzioni chiamate
    // controllo della porta
    if((functret = portValidate(portstring)) != 0){
        if(functret == 1){
            fprintf(stderr,"Errore, la porta specificata non e' valida.\n");
            return 1;
        } else if(functret == 2){
            fprintf(stderr,"Errore, la porta specificata e' minore di 5001.\n");
            return 1;
        } else if(functret == 3){
            fprintf(stderr,"Errore, la porta specificata e' maggiore di 65535.\n");
            return 1;
        }
        
    }
    // controllo dell'indirizzo IP
    if((functret = ipValidate(ipAddress)) != 0){
        if(strcmp(ipAddress,"null") != 0){ // se l'utente ha inserito null verrà automaticamente inserito l'indirizzo della macchina dove si sta runnando il server
            fprintf(stderr,"Errore, l'indirizzo IP %s non e' valido.\n",ipAddress);
            return 1;
        }
    }
    
    int port = strtol(portstring, NULL, 10);

    struct sockaddr_in serverAddr;

    //creazione del socket del server
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Errore durante la creazione del socket");
        return 1;
    }

    // Configurazione dell'indirizzo del server
    serverAddr.sin_family = AF_INET;
    if(functret != 0){
        // Assegnazione diretta del valore numerico INADDR_ANY a serverAddr.sin_addr.s_addr
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        serverAddr.sin_addr.s_addr = inet_addr(ipAddress);
    }
    serverAddr.sin_port = htons(port);

    // Binding del socket
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Errore durante il binding");
        return 1;
    }

    if (listen(serverSocket, BACKLOG) < 0) { //BACKLOG numero massimo di connessioni in contemporanea sulla stessa socket
        perror("Errore nella listen");
        return 1;
    }

    printf("il server ascolta su IP = %s e porta = %d\n",inet_ntoa(serverAddr.sin_addr),ntohs(serverAddr.sin_port));
    return 0;
}

/* Questa funzione controlla che l'input utente della porta sia veramente un numero e che sia maggiore di 5000 e minore di 65536 poiche' rientrano nel range non privilegiato di porte per uso personale */
int portValidate(const char *string){
    // trasforma la stringa in numero
    char *endptr;
    int port = strtol(string, &endptr, 10);
    //controlliamo che l'input era un numero valido
    if(*endptr != '\0'){
        return 1;
    } else if(port < 5001){
        return 2;
    } else if(port > 65535){
        return 3;
    }
    return 0;
}

/* Questa funzione controlla che l'input utente dell'indirizzo IP sia valido */
int ipValidate(const char *ipAddress) {
    struct sockaddr_in sa;
    if(inet_pton(AF_INET, ipAddress, &(sa.sin_addr))!=1){
        return 1;
    }
    return 0;
}


/* funzione che inizializza la libreria di criptazione */
int initCrypto(){
    if (sodium_init() < 0) {
        perror("problemi inizializzazione libreria libsodium");
        return 1;
    }

    return 0;
}

//funzione main dei thread
void *mainThread(void *clientSocket) {

    int socket = *((int *)clientSocket); //cast del descrittore della socket
    char user[MAX_ID]; //buffer che identifica il thread per un determinato user loggato lato client
    // stringhe per la criptazione della socket con il client
    unsigned char server_pk[crypto_kx_PUBLICKEYBYTES], server_sk[crypto_kx_SECRETKEYBYTES];
    unsigned char server_rx[crypto_kx_SESSIONKEYBYTES], server_tx[crypto_kx_SESSIONKEYBYTES];
    unsigned char client_pk[crypto_kx_PUBLICKEYBYTES];

    // scambio di chiavi per la criptazione dei dati
    if(key_exchange(server_pk, server_sk, server_rx, server_tx, client_pk, socket) != 0){
        printf("non è stato possibile stabilire una connessione sicura");
        close(socket);
        pthread_exit(NULL);
    }

    //ricezione operazione di autenticazione
    int op;
    if(receive_encrypted_int(socket,&op,1,server_rx) != 0){
        fprintf(stderr,"Errore impossibile ricevere operazione di autenticazione dal client\n");
        close(socket);
        pthread_exit(NULL);
    }
    // autenticazione del client
    if(authentication_client(user,op,server_rx,server_tx,socket)!=0){
        printf("errore nell'autenticazione del client \n");
        close(socket);
        pthread_exit(NULL);
    }


    close(socket);
    pthread_exit(NULL);
}


//la funzione serve per scambiare le chiavi crittografiche fra il client e il thread associato
int key_exchange(unsigned char* server_pk, unsigned char* server_sk, unsigned char* server_rx, unsigned char* server_tx, unsigned char* client_pk, int socket){
    // Genera le chiavi del server privata e pubblica
    crypto_kx_keypair(server_pk, server_sk);
    // Invia la chiave pubblica del server al client
    if(send_data(server_pk,crypto_kx_PUBLICKEYBYTES,socket) != 0){
        printf("errore nell'invio del server_pk \n");
        return 1;
    }
    // Riceve la chiave pubblica del client al server
    if(receive_data(client_pk,crypto_kx_PUBLICKEYBYTES,socket) != 0){
        printf("errore nella ricezione del client_pk \n");
        return 1;
    }
    // Calcola una coppia di chiavi per criptazione e decriptazione dei dati
    if (crypto_kx_server_session_keys(server_rx, server_tx, server_pk, server_sk, client_pk) != 0) {
        printf("Errore nel creare la coppia di chiavi per ricezione e invio \n");
        return 1;
    }
    return 0;
}


// La funzione gestisce le autenticazioni dei client (registrandoli o controllando le credenziali)
int authentication_client(char* user, int op, unsigned char* server_rx,unsigned char* server_tx,int socket){
    Utente* utente; //puntatore ad un nodo utente
    // ricezione dell'username
    char username[MAX_ID]; //puntatore al buffer contenente l'username inviato dall'utente
    int instr_server = -1; // risposta del server al client (-1 stato default)
    while( instr_server != 0){
        // riceve la stringa username scritta dall'utente
        if(receive_encrypted_data(socket,(unsigned char*)username,MAX_ID,server_rx) != 0){
            printf("errore ricezione username dal client \n");
            return 1;
        }
        printf("username: %s\n",username);
        //cerchiamo all'interno della lista se esiste un utente con quell'username
        if((utente = searchInHashTable(tableUser,username)) != NULL){
            if(op != 1){
                //l'utente vuole loggarsi
                instr_server = 0; // confermiamo l'esistenza dell'utente
                strcpy(user,username); //inserisco l'username all'interno del buffer che identifica l'utente nel thread
            } else {
                //l'utente vuole registrarsi
                instr_server = 1; //username gia esistente
            }
        } else {
            if(op == 1){
                //l'utente vuole registrarsi
                instr_server = 0; // confermiamo username valido
                strcpy(user,username); //inserisco l'username all'interno del buffer che identifica l'utente nel thread
                //allochiamo memoria per un nuovo utente
                utente = (Utente*)malloc(sizeof(Utente));
                if(utente == NULL){
                    perror("errore nella malloc per la creazione di un nuovo utente");
                    return 1;
                }
                strcpy(utente->username,username);
            } else{
                //l'utente vuole loggarsi
                instr_server = 2; // username insesistente
            }
        }
        // invio risultato della ricerca al client
        send_encrypted_int(socket,instr_server,server_tx);
    }
    printf("username salvato: %s \n",utente->username);
    //ricezione della password
    char* pswdsaved;
    if(op != 1){
        // l'utente deve loggarsi quindi recuperiamo la sua password
        pswdsaved = PswdSaved(*utente);
    }
    char password[MAX_PSWD]; // stringa temporanea per la password inviata dal cliente
    char hashed_password[ENC_PSWD]; // stringa che contiene la password cifrata
    instr_server = -1; // riportiamo l' "instruzione lato server" allo stato di default
    // Blocca la memoria riservata alla password in chiaro
    if (sodium_mlock(password, sizeof(password)) != 0) {
        fprintf(stderr, "Impossibile bloccare la memoria riservata alla password\n");
        return 1;
    }
    while(instr_server != 0){
        // riceve la password in chiaro inviata dall'utente
        if(receive_encrypted_data(socket,(unsigned char*)password,MAX_PSWD,server_rx)!=0){
            printf("errore nella ricezione della password in chiaro dal server \n");\
            return 1;
        }
        printf("password ricevuta: %s \n",password);
        if(op == 1){
            //l'utente vuole registrarsi
            // Cifra la password in chiaro
            if (crypto_pwhash_str(hashed_password, password, strlen(password), crypto_pwhash_OPSLIMIT_SENSITIVE, crypto_pwhash_MEMLIMIT_SENSITIVE) != 0) {
                perror("error to hash password ");
                return 1;
            }
            if( regUser(*utente, hashed_password) != 0){
                fprintf(stderr,"Errore: impossibile registrare il nuovo utente\n");
                return 1;
            }
            instr_server = 0;
        } else {
            //l'utente vuole loggarsi o eliminare l'account
            
            if ( crypto_pwhash_str_verify(pswdsaved, password, strlen(password)) != 0) {
                // Password sbagliata
                instr_server = 1; // messaggio al client "password errata" 
            } else {
                instr_server = 0; //messaggio al client "password corretta"
            }
        }

        //eliminiamo l'account se abbiamo selezionato questa opzione
        if(op == 2){
            if(deleteUser(*utente) != 0){
                fprintf(stderr,"Errore impossibile eliminare l'account \n");
                return 1;
            }
        }
        // Invia risultato al client
        send_encrypted_int(socket,instr_server,server_tx);
    }

    
    //sblocco la memoria riservata alla password in chiaro
    sodium_munlock(password, sizeof(password));

    return 0;
}
/*
// fuznione che preleva la password salvata sul file credenziali
char* PswdSaved(Utente user){
    //apriamo il file credenziali
    FILE* file = fopen(CREDPATH,"r");
    if(file == NULL){
        fprintf(stderr,"Impossibile aprire il file credenziali\n");
        return NULL;
    }
    //posizioniamo il seek nella tupla esatta
    if (fseek(file, user.pos, SEEK_SET) != 0) {
        perror("Errore nel posizionamento del puntatore di posizione");
        fclose(file);
        return NULL;
    }
    // leggiamo la password
    char* password = malloc(sizeof(char)*ENC_PSWD);
    if(password == NULL){
        perror("Errore malloc:");
        return NULL;
    }
    if( fscanf(file,"%*d %*s %s[^\n]",password) == EOF){
        perror("Errore nella lettura della password utente");
        fclose(file);
        return NULL;
    }
    fclose(file);
    return password;
}
*/
// parte nuova


// la funzione inizializza le due hashtable e i due file di archiviazione di credenziali e chat  creando delle tabelle vuote e eliminando dai file le righe invalidate
int initDataBase(){
    /* Parte istanziata nell'address space */
    //instanziamo spazio per le tabelle hash
    utenteTable = (HashNode*)malloc(sizeof(HashNode)*USER_HASH_SIZE);
    if(utenteTable == NULL){
        fprintf(stderr,"Errore malloc utenteTable\n");
        return 1;
    }
    chatTable = (HashNode*)malloc(sizeof(HashNode)*CHAT_HASH_SIZE);
    if(chatTable == NULL){
        fprintf(stderr,"Errore malloc charTable\n");
        free(utenteTable);
        return 1;
    }
    
    //inizializziamo ogni riga delle tabelle a null
    for(int i=0;i<USER_HASH_SIZE;i++){
        utenteTable[i].head = NULL;
        if (pthread_mutex_init(&(utenteTable[i].modify), NULL) != 0) {
        fprintf(stderr, "Errore nell'inizializzazione dei mutex in Hashtable initialize\n");
        return 1;
        }
    }
    for(int i=0;i<CHAT_HASH_SIZE;i++){
        chatTable[i].head = NULL;
        if (pthread_mutex_init(&(chatTable[i].modify), NULL) != 0) {
        fprintf(stderr, "Errore nell'inizializzazione dei mutex in Hashtable initialize\n");
        return 1;
        }
    }

    /* Parte su File*/
    // creazione di un mutex per la scrittura sul file credenziali
    if( pthread_mutex_init(&(writecredential),NULL) != 0){
        perror("Errore nell'inizializzazione del mutex per il file credenziali");
        return 1;
    }
    if( pthread_mutex_init(&(writechat),NULL) != 0){
        perror("Errore nell'inizializzazione del mutex per il file chat");
        return 1;
    }
    // inizializzazione del file credenziali
    while(initFile(FILECRED) != 0){
        fprintf(stderr,"Errore nell'inizializzazione del file credenziali\n");
        return 1;
    }
    if(initFile(FILECHAT) != 0){
        fprintf(stderr,"Errore impossibile inizializzare il file delle chat\n");
        return 1;
    }
    return 0;
}

// Funzione hash per la gestione degli indici per le tabelle
unsigned int hash_function(char *str, int hash_size) {
    unsigned int hash = 0;
    while (*str) {
        hash = (hash * 31) + (*str++);
    }
    return hash % hash_size;
}

// Funzione di confronto per la struttura Utente
int compareUtente(const void *key, const void *node) {
    return strcmp((const char *)key, ((const struct Utente *)node)->username);
}

// Funzione di confronto per la struttura Chat
int compareChat(const void *key, const void *node) {
    return strcmp((const char *)key, ((const struct Chat *)node)->id_chat);
}

// Funzione generica per aggiungere un nodo alla tabella hash
int addNode(HashNode *hashTable, char *key, void *data, int hash_size, size_t structSize) {
    // troviamo indice per la tabella in cui inserire il nuovo nodo
    unsigned int i = hash_function(key, hash_size);

    // Creazione del nuovo nodo
    void *newNode = malloc(structSize);
    if (newNode == NULL) {
        fprintf(stderr, "Errore malloc per il nuovo nodo\n");
        return 1;
    }

    // Inizializzazione del nuovo nodo con i dati forniti
    memcpy(newNode, data, structSize);

    //aggiungiamo il nodo nella lista corretta
    if(hashTable[i].head == NULL){
        // la lista è vuota
        //blocchiamo la scrittura sulla lista
        if(pthread_mutex_lock(&(hashTable[i].modify)) != 0){
            fprintf(stderr,"Errore impossibile bloccare la scrittura sulla lista\n");
            free(newNode);
            return 1;
        }
        //aggiungiamo il nodo in testa
        hashTable[i].head = newNode;
        //rilasciamo la scrittura sulla lista
        if(pthread_mutex_unlock(&(hashTable[i].modify)) != 0){
            fprintf(stderr,"Errore impossibile sbloccare la scrittura sulla lista\n");
            return 1;
        }
    } else {
        // la lista non è vuota
        //scorriamo la lista fino ad arrivare alla fine
        void *current = hashTable->head;
        while (*((void **)(&current->next)) != NULL) {
            current = *((void **)(&current->next));
        }

        //blocchiamo la scrittura sull'ultimo puntatore
        if(pthread_mutex_lock(&(current->modify)) != 0){
            fprintf(stderr,"Errore impossibile bloccare la scrittura sull'ultimo nodo\n");
            free(newNode);
            return 1;
        }
        // Inseriamo il nodo dopo l'ultimo
        *((void **)(&current->next)) = newNode;

        //sblocchiamo la scrittura sull'ultimo nodo
        if(pthread_mutex_unlock(&(current->modify)) != 0){
            fprintf(stderr,"Errore impossibile sbloccare la scrittura sull'ultimo nodo\n");
            return 1;
        }
    }

    return 0;
}

// Funzione generica per cercare un nodo nella tabella hash restituisce 0 se lha trovato 1 se c'`e stato errore e 2 se non è stato trovato
int findNode(HashNode *hashTable, char *key, int hash_size, int (*compare)(const void *, const void *), void* node) {
    //troviamo la riga della tabella
    unsigned int i = hash_function(key, hash_size);

    // Scorrimento della lista collegata in quella posizione della tabella hash
    if(pthread_mutex_lock(&(hashTable[i].modify)) != 0){
        fprintf(stderr,"Errore impossibile prelevare mutex per prendere la testa della lista\n");
        return 1;
    }
    void *current = (void*)hashTable[i].head;
    if(pthread_mutex_unlock(&(hashTable[i].modify)) != 0){
        fprintf(stderr,"Errore impossibile rilasciare mutex dopo aver preso la testa della lista\n");
        return 1;
    }
    void *prev = NULL;
    while (current != NULL) {
        // Confronto della chiave per trovare il nodo desiderato
        if (compare(key, current) == 0) {
            node = current;
            return 0;
        }
        // Passiamo al prossimo nodo nella lista
        // prima prendiamo il mutex per ottenere un campo next valido
        if(pthread_mutex_lock(&(current->modify)) != 0){
            fprintf(stderr,"Errore impossibile prelevare mutex per passare al nodo successivo\n");
            return 1;
        }
        prev = current;
        current = prev->next;
        if(pthread_mutex_unlock(&(prev->modify)) != 0){
            fprintf(stderr,"Errore impossibile rilasciare il mutex dopo essere passati al nodo successivo\n");
            return 1;
        }
    }
    //non è stato trovato il nodo all'interno della struttura dati allora bisogna cercarlo nelle credenziali se un nodo utente
    return 2; // Nodo non trovato
}

// Funzione generica per eliminare un nodo dalla tabella hash
int deleteNode(HashNode *hashTable, char *key, int hash_size, int (*compare)(const void *, const void *)) {
    //troviamo indice tabella
    unsigned int i = hash_function(key, hash_size);

    // Scorrimento della lista collegata in quella posizione della tabella hash
    if(pthread_mutex_lock(&(hashTable[i].modify)) != 0){
        fprintf(stderr,"Errore impossibile prelevare mutex per prendere la testa della lista\n");
        return 1;
    }
    void *current = (void*)hashTable[i].head;
    if(pthread_mutex_unlock(&(hashTable[i].modify)) != 0){
        fprintf(stderr,"Errore impossibile rilasciare mutex dopo aver preso la testa della lista\n");
        return 1;
    }
    void *prev = NULL;

    // Scorrimento della lista collegata in quella posizione della tabella hash
    while (current != NULL) {
        // Confronto della chiave per trovare il nodo desiderato
        if (compare(key, current) == 0) {
            // blocchiamo la lettura scrittura sul nodo da eliminare
            if(pthread_mutex_lock(&(current->modify)) != 0){
                fprintf(stderr,"Errore imposibile bloccare mutex del nodo eliminato");
                return 1;
            }
            if (prev == NULL) {
                // Se il nodo da eliminare è il primo, aggiorniamo il puntatore alla testa
                //blocchiamo mutex testa lista
                if(pthread_mutex_lock(&(hashTable[i].modify)) != 0){
                    fprintf(stderr,"Errore impossibile bloccare mutex per cambio nodo in testa\n");
                    return 1;
                }
                hashTable[i].head = current->next;
                if(pthread_mutex_unlock(&(hashTable[i].modify)) != 0){
                    fprintf(stderr,"Errore impossibile sbloccare mutex dopo cambio nodo in testa\n");
                    return 1;
                }
            } else {
                // Altrimenti, eliminiamo il nodo aggiornando il puntatore del nodo precedente
                if(pthread_mutex_lock(&(prev->modify)) != 0){
                    fprintf(stderr,"Errore impossibile bloccare mutex per eliminazione nodo\n");
                    return 1;
                }
                prev->next = current->next;
                if(pthread_mutex_unlock(&(prev->modify)) != 0){
                    fprintf(stderr,"Errore impossibile sbloccare mutex dopo eliminazione nodo\n");
                    return 1;
                }
            }
            free(current); // Liberiamo la memoria del nodo eliminato
            return 0;
        }
        // Passiamo al prossimo nodo nella lista
        prev = current;
        current = prev->next;
    }
    return 1; // Nodo non trovato
}

// la funzione accetta il nome del file senza txt e lo inizializza se c'è un errore nel "travasare" i dati dal temporaneo all'originale ritorna 2
int initFile(char* nomeFile){
    //creo la stringa vera per aprire il file
    char pathfile[strlen(nomeFile)+5];
    strcpy(pathfile,nomeFile);
    strcat(pathfile,".txt");
    //apertura file originale
    FILE *originalfile = fopen(pathfile,"r+"); //apertura del file in RDWR all'inizio
    if(originalfile == NULL){
        //il file non esiste e lo crea
        originalfile = fopen(pathfile,"w");
        fclose(originalfile);
        return 0;
    }
    //crea il duplicato
    char pathtemp[strlen(pathfile)+strlen("temp")+1];
    strcpy(pathtemp,"temp");
    strcat(pathtemp,pathfile);
    FILE *dupfile = fopen(pathtemp,"w+"); //apertura in scrittura troncata 
    if(dupfile == NULL){
        fprintf(stderr,"Errore: non e' stato possibile aprire il file duplicato \n");
        fclose(originalfile);
        return 1;
    }
    int temp;
    while((temp = fgetc(originalfile)) != EOF){
        fputc(temp,dupfile);
    }
    // spostiamo il seek del dupfile all'inizio del file
    fseek(dupfile, 0, SEEK_SET);
    //chiudiamo il file orginale per riaprlirlo troncato
    fclose(originalfile);
    originalfile = fopen(pathfile,"w+");
    if(originalfile == NULL){
        fprintf(stderr,"Errore: non e' stato possibile aprire il file originale \n");
        fclose(dupfile);
        return 1;
    }
    // riscrive nel file originale solo le credenziali valide
    // leggo il primo carattere di ogni riga
    while ((temp = fgetc(dupfile)) != EOF) {
        // Verifica il primo carattere
        if (temp == '1') { // Se il primo carattere è '1'
            // copia la riga nel file originale
            fputc(temp,originalfile); // scriviamo il bit validate
            while(((temp = fgetc(dupfile)) != '\n') || (temp == EOF)){
                if(temp == EOF){
                    fprintf(stderr,"Errore inizializzazione file esso verrà distrutto e ricreato perdendo tutti i dati\n");
                    return 2;
                }
                fputc(temp,originalfile);
            }
            fputc(temp,originalfile); //scriviamo il carattere nuova riga
        }
        // Altrimenti, se è '0', la riga viene saltata
        while(((temp = fgetc(dupfile)) != '\n') || (temp == EOF)){
            if(temp == EOF){
                fprintf(stderr,"Errore inizializzazione file esso verrà distrutto e ricreato perdendo tutti i dati\n");
                return 2;
            }
        }
    }
    // Chiude entrambi i file
    fclose(dupfile);
    fclose(originalfile);
    //per sicurezza viene eliminato il file temporaneo
    if(remove(pathtemp) != 0){
        perror("Errore, impossibile eliminare il file temporaneo");
        return 1;
    }
    return 0;
}

int writeChat(char* id_chat, int num_part, char** part){
    //blocca il mutex sul file chat
    if(pthread_mutex_lock(&(writechat)) != 0){
        perror("Errore impossibile bloccare mutex per scrivere nel file chat");
        return 1;
    }
    // apriamo il file chat in scrittura
    char pathfile[strlen(FILECHAT)+5];
    strcpy(pathfile,FILECHAT);
    strcat(pathfile,".txt");
    FILE* file = fopen(pathfile,"a");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire il file in modalità append\n");
        return 1;
    }
    //scriviamo la nuova riga sul file
    fprintf(file,"1 %s %d",id_chat,num_part);
    for(int i=0;i<num_part;i++){
        fprintf(file," %s",part[i]);
    }
    fprintf(file,"\n");
    fflush(stdout);
    //rilasciamo il mutex
    if(pthread_mutex_unlock(&(writechat)) != 0){
        perror("Errore impossibile rilasciare il mutex per scrivere nel file chat");
        return 1;
    }
    //chiudiamo il file
    fclose(file);
    return 0;
}

int writeCredential(char* username, char* password){
    //blocchiamo il mutex del file credential
    if(pthread_mutex_init(&(writecredential),NULL) != 0){
        perror("Errore impossibile bloccare il mutex per la scrittura sul file credenziali");
        return 1;
    }
    //apriamo il file credenziali
    char pathfile[strlen(FILECRED)+5];
    strcpy(pathfile,FILECRED);
    strcat(pathfile,".txt");
    FILE* file = fopen(pathfile,"a");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire il file in modalità append\n");
        return 1;
    }
    //scriviamo la nuova riga sul file
    fprintf(file,"1 %s %s\n",username,password);
    fflush(stdout);
    //sblocchiamo il mutex
    if(pthread_mutex_unlock(&(writecredential)) != 0){
        perror("Errore impossibile rilasciare il mutex per scrivere nel file credenziali");
        return 1;
    }
    //chiudiamo il file
    fclose(file);
    return 0;
}
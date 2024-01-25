#include "server2.h"

struct semFile chatsem;
struct semFile usersem;

Node* chatTable[MAX_TABLE];
Node* userTable[MAX_TABLE];
int serverSocket; // descrittore della socket lato server

// funzione di partenza per il server
int main(int argc, char **argv){
    // controlliamo che il programma sia in running per conto dell'amministratore del sistema (i file sono stati creati in modo tale da essere letti solamente dall'amministratore)
    if (geteuid() != 0) {
        fprintf(stderr, "Il programma deve essere eseguito con privilegi di amministratore (root).\n");
        exit(EXIT_FAILURE);
    }
    // controllo inserimento dei parametri all'avvio del server [ip address] [port]
    if(argc != 3){
        fprintf(stderr,"Usare: sudo %s [IP address] (if IP=null is the same of machine) [port]\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    // inizializziamo la gestione dei segnali
    if(initSignal() != 0){
        fprintf(stderr,"errore nell'inizializzazione dei segnali\n");
        exit(EXIT_FAILURE);
    }
    // inizializziamo la socket per la connessione con i client
    if(initSocket(argv[1], argv[2]) != 0){
        fprintf(stderr,"errore nell'inizializzazione della socket\n");
        exit(EXIT_FAILURE);
    }
    // inizializzazione della libreria per la crittografia dei dati trasfariti tramite socket e per il salvataggio delle password sul file Credenziali
    if(initCrypto() != 0){
        fprintf(stderr,"Errore nell'inizializzazione della libreria crittografica\n");
        closeServer();
        exit(EXIT_FAILURE);
    }
    // Inizializzazione dei file Credenziali e Chats e creazione delle tabelle hash per Utenti e Chat
    if(initDataBase() != 0){
        fprintf(stderr,"Errore: impossibile inizializzare i database\n");
        closeServer();
        exit(EXIT_FAILURE);
    }
    // Sistema configurato correttamente
    printf("Server configurato correttamente.\n");
    // Setting del sistema per essere pronto ad accettare nuove connessioni da parte dei client
    while(1){
        //socket in stato di accettazione
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
    }
    closeServer();
    return 0;
}


/* Funzione che chiude il server correttamente in caso di trigger segnali*/
void signalclose(){
    closeServer();
    printf("\n Il server e' stato chiuso correttamente\n");
    exit(EXIT_SUCCESS);
}


/* Questa funzione serve a chiudere correttamente tutte le componenti del server */
void closeServer(){
    close(serverSocket);
}


/* funzione che inizializza la gestione dei sengali in modo tale da gestire i segnali SIGINT, SIGTERM, SIGPIPE */
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


/* funzione per l'inizializzazione della socket, la funzione crea la socket ad un determinato ip e porta inserita come parametro ma non la mette in ascolto */
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
    char username[MAX_ID]; //buffer che identifica il thread per un determinato user loggato lato client
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
    //ricezione dell'operazione di autenticazione
    int op;
    if(receive_encrypted_int(socket,&op,1,server_rx) != 0){
        fprintf(stderr,"Errore impossibile ricevere operazione di autenticazione dal client\n");
        close(socket);
        pthread_exit(NULL);
    }
    printf("operazione selezionata: %d \n",op);
    //ricezione username
    int resp = 1;
    Utente* user = NULL;
    while(resp != 0){
        //aspettiamo l'username dal client
        if(receive_encrypted_data(socket,(unsigned char*)username,MAX_ID,server_rx) != 0){
            fprintf(stderr,"Errore impossibile ricevere username dal client\n");
            close(socket);
            pthread_exit(NULL);
        }
        printf("username ricevuto: %s\n",username);
        if(op != 1){
            // fase di accesso
            Node* node = NULL;
            if((node=searchNode(userTable,username,compareUtente)) != NULL){ 
                //user trovato nella tabella hash
                user = (Utente*)node->content;
                resp = 0;
            } else if(findUtente(username) == 0){
                //user trovato nel file
                if((node = searchNode(userTable,username,compareUtente)) != NULL){
                    user = (Utente*)node->content;
                    resp = 0;
                }
            } else {
                // username già in uso
                resp = 1;
            }
        } else {
            // fase di registrazione
            if(searchNode(userTable,username,compareUtente) != NULL){
                resp = 2;
            } else {
                if(findUtente(username) == 2){
                    // username ancora non utilizzato
                    resp = 0;
                } else {
                    // username già esistente
                    resp = 2;
                } 
            }
        }
        //inviamo risposta al client
        if(send_encrypted_int(socket,resp,1,server_tx) != 0){
            fprintf(stderr,"Errore nell'invio della rispsota\n");
            close(socket);
            pthread_exit(NULL);
        }
    }
    //ricezione password
    char password[MAX_PSWD]; // stringa temporanea per la password inviata dal cliente
    char hashed_password[MAX_ENCPSWD]; // stringa che contiene la password cifrata
    resp = -1; // riportiamo l' "instruzione lato server" allo stato di default
    // Blocca la memoria riservata alla password in chiaro
    if (sodium_mlock(password, sizeof(password)) != 0) {
        fprintf(stderr, "Impossibile bloccare la memoria riservata alla password\n");
        close(socket);
        pthread_exit(NULL);
    }
    if(op != 1){
        //prelevo la password dal file
        if(findPswd(user->seek,hashed_password) != 0){
            fprintf(stderr,"Errore prelievo password dal file credenziali\n");
            close(socket);
            pthread_exit(NULL);
        }
    }
    while(resp != 0){
        //riceviamo la password
        if(receive_encrypted_data(socket,(unsigned char*)password,MAX_PSWD,server_rx)!=0){
            printf("errore nella ricezione della password in chiaro dal server \n");\
            close(socket);
            pthread_exit(NULL);
        }
        if(op != 1){
            // login
            // verifica la password
            if ( crypto_pwhash_str_verify(hashed_password, password, strlen(password)) != 0) {
                // Password sbagliata
                resp = 1; // messaggio al client "password errata" 
            } else {
                resp = 0; //messaggio al client "password corretta"
            }
        } else {
            // registrazione utente
            // cifriamo la passowrd
            if (crypto_pwhash_str(hashed_password, password, strlen(password), crypto_pwhash_OPSLIMIT_SENSITIVE, crypto_pwhash_MEMLIMIT_SENSITIVE) != 0) {
                perror("error to hash password ");
                close(socket);
                pthread_exit(NULL);
            }
            // aggiungiamo il nuovo utente
            if(regUtente(username,hashed_password) != 0){
                fprintf(stderr,"Errore nella fase di resgistrazione del nuovo utente\n");
                close(socket);
                pthread_exit(NULL);
            }
            resp = 0;
        }
        //inviamo risposta al client
        if(send_encrypted_int(socket,resp,1,server_tx) != 0){
            fprintf(stderr,"Errore nell'invio della rispsota\n");
            close(socket);
            pthread_exit(NULL);
        }
    }
    //sblocco la memoria riservata alla password in chiaro
    sodium_munlock(password, sizeof(password));
    if(op == 2){
        //l'utente vuole eliminare l'account
        if(rmNode(userTable,username,compareUtente,rmUtente) != 0){
            fprintf(stderr,"Errore nell'eliminazione dell'utente\n");
            close(socket);
            pthread_exit(NULL);
        }
    }

    // Fase principale
    while(1){
        op = -1; // resettiamo il valore dell'operazione a default
        // riceviamo operazione da eseguire
        if(receive_encrypted_int(socket,&op,1,server_rx) != 0){
            fprintf(stderr,"Errore impossibile ricevere operazione di autenticazione dal client\n");
            close(socket);
            pthread_exit(NULL);
        }
        printf("operazione scelta: %d\n",op);
        if(op == 0){
            // crea nuova chat e la apre
            if( createChat() != 0){
                fprintf(stderr,"Errore impossibile creare la chat richiesta\n");
                close(socket);
                pthread_exit(NULL);
            }
        } else if(op == 3){
            // chiudiamo la connessione con il client
            close(socket);
            pthread_exit(NULL);
        } else if (op == 1){
            // apriamo uina chat esistente
            if(openChat() != 0){
                fprintf(stderr,"Errore nell'aprire una chat\n");
                close(socket);
                pthread_exit(NULL);
            }
        } else if( op == 2){
            if(deleteChat() != 0){
                fprintf(stderr,"Errore nell'eliminare una chat\n");
                close(socket);
                pthread_exit(NULL);
            }
        }
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

// questa funzione ha lo scopo di aggiornare tutti i file mantenedno solo le tuple valide e di creare le tabelle vuote per utenti e chat
int initDataBase(){
    // creiamo la cartella delle chat se non esiste
    if(mkdir(CHAT_FOLDER,S_IRWXU) != 0){
        if(errno != EEXIST){
            perror("Errore nella creaazione della chat folder");
            return 1;
        }
    }
    //inizializziamo il file delle credenziali
    if(initFile(FILECRED) != 0){
        fprintf(stderr,"Errore impossibile inizializzare il file credenziali\n");
        return 1;
    }
    if(pthread_mutex_init(&(usersem.main),NULL) != 0){
        perror("Errore nell'inizializzazione del mutex per il file credentials");
        return 1;
    }
    if(sem_init(&chatsem.readers,0,0) != 0){
        perror("Errore nell'inizializzazione del semaforo per i readers in file chat");
        return 1;
    }
    //inizializziamo il file chat
    if(initFile(FILECHAT) != 0){
        fprintf(stderr,"Errore impossibile inizializzare il file chat\n");
        return 1;
    }
    if(pthread_mutex_init(&(chatsem.main),NULL) != 0){
        perror("Errore nell'inizializzazione del mutex per il file chat");
        return 1;
    }
    if(sem_init(&usersem.readers,0,0) != 0){
        perror("Errore nell'inizializzazione del semaforo per i readers in file credential");
        return 1;
    }
    //inizializziamo la tabella hash per le chat
    for(int i=0;i<MAX_TABLE;i++){
        chatTable[i] = malloc(sizeof(Node));
        if(chatTable[i] == NULL){
            perror("Errore nell'inizializzazione della hash table per le chat");
            return 1;
        }
        chatTable[i]->content = NULL;
        if(pthread_mutex_init(&(chatTable[i]->modify),NULL) != 0){
            perror("Errore nell'inizializzazione del mutex nella tabella chatTable");
            return 1;
        }
    }
    //inizializziamo la tabella hash per le credenziali utenti
    for(int j=0;j<MAX_TABLE;j++){
        userTable[j] = malloc(sizeof(Node));
        if(userTable[j] == NULL){
            perror("Errore nell'inizializzazione della hash table per le chat");
            return 1;
        }
        userTable[j]->content = NULL;
        if(pthread_mutex_init(&(userTable[j]->modify),NULL) != 0){
            perror("Errore nell'inizializzazione del mutex nella tabella userTable");
            return 1;
        }
    }
    return 0;
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
        int filedescriptor = open(pathfile, O_CREAT, S_IRUSR | S_IWUSR);
        if( filedescriptor == -1){
            perror("Errore nella creazione del file");
            return 1;
        }
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
                    fprintf(stderr,"Errore durante la copia di una tupla valida\n");
                    fclose(originalfile);
                    fclose(dupfile);
                    return 1;
                }
                fputc(temp,originalfile);
            }
            fputc(temp,originalfile); //scriviamo il carattere nuova riga
        } else {
            // Altrimenti, se è '0', la riga viene saltata
            while(((temp = fgetc(dupfile)) != '\n') || (temp == EOF)){
                if(temp == EOF){
                    fprintf(stderr,"Errore durante lo scarto di una tupla non valida\n");
                    fclose(originalfile);
                    fclose(dupfile);
                    return 1;
                }
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

// Hash function
unsigned int hash_function(char* key) {
    unsigned int hash = 0;
    while (*key) {
        hash = (hash * 31) + (*key++);
    }
    return hash % MAX_TABLE;
}

// Funzione di confronto per Chat
int compareChat(const void* a, const char* key) {
    return strcmp(((Chat*)a)->chat_id, key);
}

// Funzione di confronto per Utente
int compareUtente(const void* a, const char* key) {
    return strcmp(((Utente*)a)->username, key);
}

//funzione per liberare una struttura chat
int rmChat(void* chat){
    //cast del puntatore
    Chat* chatptr = (Chat*)chat;
    //liberiamo la memoria allocata per il chat_id
    free(chatptr->chat_id);
    //distruggiamo il semaforo per scrivere un messaggio
    if(pthread_mutex_destroy(&(chatptr->sem.main)) != 0){
        perror("Errore nella distruzione del mutex per la scrittura di un file in fase di eliminazione");
        return 1;
    }
    if(sem_destroy(&(chatptr->sem.readers)) != 0){
        perror("Errore nella distruzione del semaforo per i lettori del file");
        return 1;
    }
    //liberiamo la memoria riservata alla struttura dati chat
    free(chatptr);
    return 0;
}

//funzione per liberare la memoria di una struttura dati Utente e invalidare la riga nel file crednziali
int rmUtente(void* user){
    //cast del puntatore
    Utente* userptr = (Utente*)user;
    // invalidiamo la riga nel file credenziali
    if(startWriteFile(&usersem) != 0){
        fprintf(stderr,"Errore blocco semaforo in scrittura sul file crednziali\n");
        return 1;
    }
    char pathfile[strlen(FILECRED)+5];
    strcpy(pathfile,FILECRED);
    strcat(pathfile,".txt");
    FILE* file = fopen(pathfile,"r+");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire il file crednziali\n");
        return 1;
    }
    if(fseek(file,userptr->seek,SEEK_SET) != 0){
        perror("Errore nel posizionamento del seek");
        fclose(file);
        return 1;
    }
    fprintf(file,"0");
    fclose(file);
    if(endWriteFile(&usersem) != 0){
        fprintf(stderr,"Errore nel rilascio del mutex scrittura credenziali\n");
        return 1;
    }
    //liberiamo la memoria per riservata per l'username
    free(userptr->username);
    free(userptr);
    return 0;
}

//funzione per eliminare un nodo da una tabella hash
int rmNode(Node** table, char* key, int (*compare)(const void*, const char*), int (*remove)(void *)){
    //calcoliamo l'indice della riga
    unsigned int i = hash_function(key);
    //facciamo una ricerca del nodo da eliminare salvando anche il puntatore al precedente
    Node* prev = NULL;
    Node* current = table[i];
    while( current != NULL){
        //blocchiamo il mutex per la modifica
        if(pthread_mutex_lock(&(current->modify)) != 0){
            perror("Errore nello bloccare il mutex del nodo corrente");
            return 1;
        }
        //se current è il primo nodo passiamo al successivo
        if(current == table[i]){
            prev = current;
            current = prev->next;
        } else {
            if(compare(current->content,key) == 0){      //controlliamo se è il nodo giusto
                //current è il nodo che dobbiamo eliminare
                //eliminiamo il contenuto
                if(remove(current->content) != 0){
                    fprintf(stderr,"Errore durante l'eliminazione del contenuto del nodo\n");
                    return 1;
                }
                //spostiamo i puntatori
                prev->next = current->next;
                //facciamo la free del nodo corrente da eliminare
                free(current);
                //rilasciamo il mutex del precedente
                if(pthread_mutex_unlock(&(prev->modify)) != 0){
                    perror("Errore nel rilascio del mutex dopo eliminazione nodo successivo");
                    return 1;
                }
                return 0;
            }
            //liberiamo il mutex del nodo precedente
            if(pthread_mutex_unlock(&(prev->modify)) != 0){
                    perror("Errore nel rilascio del mutex nello scorrimento della lista");
                    return 1;
            }
            //ci spostiamo di un nodo avanti
            prev = current;
            current = prev->next;
        }
    }
    if(pthread_mutex_unlock(&(prev->modify)) != 0){
        perror("Errore nel rilascio del mutex dopo aver letto tutta la lista");
        return 1;
    }
    fprintf(stderr,"Errore il nodo da eliminare non e' stato trovato\n");
    return 2;
}

// funzione che cerca un nodo all'interno della sua hashtable
Node* searchNode(Node** table, char* key, int (*compare)(const void*, const char*)){
    //troviamo indice della riga
    unsigned int i = hash_function(key);
    //scorriamo la lista per trovare il nodo richiesto
    Node* current = table[i]->next;
    while(current != NULL){
        if(compare(current->content,key) == 0){
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// funzione che crea un nuovo nodo per una determinata struttura dati e lo aggiunge un testa alla lista
int addNode(Node** table, char* key, void* data){
    // troviamo indice per la riga
    unsigned int i = hash_function(key);
    //creiamo un nuovo nodo
    Node* newNode = malloc(sizeof(Node));
    if(newNode == NULL){
        fprintf(stderr,"Errore nella creazione di un nuovo nodo\n");
        return 1;
    }
    newNode->content = data;
    if(pthread_mutex_init(&(newNode->modify),NULL) != 0){
        perror("Errore inizializzazione mutex per il nuovo nodo creato");
        return 1;
    }
    // blocchiamo la scrittura sul primo nodo della lista
    if(pthread_mutex_lock(&(table[i]->modify)) != 0){
        perror("Errore nel blocco del mutex della testa della lista");
        return 1;
    }
    // modifichiamo i puntatori
    newNode->next = table[i]->next;
    table[i]->next = newNode;
    //rilasciamo il mutex per la testa della lista
    if(pthread_mutex_unlock(&(table[i]->modify)) != 0){
        perror("Errore nell'unlock mutex della testa della lista");
        return 1;
    }
    return 0;
}

//funzione che gestisce i mutex per la lettura su file
int startReadFile(struct semFile* sem){
    //prendiamoil mutex main
    if(pthread_mutex_lock(&(sem->main)) != 0){
        perror("Impossibile prendere il mutex main del file");
        return 1;
    }
    //inseriamo un semaforo all'interno del semaforo reader per indicare che è presente un lettore
    sem_post(&(sem->readers));
    //rilasciamo il mutex main
    if(pthread_mutex_unlock(&(sem->main)) != 0){
        perror("Impossibile rilascaire il mutex main del file");
        return 1;
    }
    return 0;
}

// funzione che gestisce la struttura dati semaforo
int endReadFile(struct semFile* sem){
    //riprendiamo il gettone del semaforo che avevamo rilasciato allo start
    sem_wait(&(sem->readers));
    return 0;
}

// funzione che gestisce la struttura dati semaforo
int startWriteFile(struct semFile* sem){
    //predniamo il mutex main
    if(pthread_mutex_lock(&(sem->main)) != 0){
        perror("Impossibile prendere il mutex main del file");
        return 1;
    }
    //aspetiamo che non ci siano più semafori reader disponibili
    while (sem_trywait(&sem->readers) == 0) {
        // Tentativo riuscito, abbimo preso un token quindi dobbiamo restituirlo
        sem_post(&(sem->readers));
    }
    // adesso siamo sicuri che non ci sono più lettori e non possono entrare poichè abbiamo il mutex main
    return 0;
}

// funzione che gestisce la struttura dati semaforo
int endWriteFile(struct semFile* sem){
    // rilasciamo il mutex main
    if(pthread_mutex_unlock(&(sem->main)) != 0){
        perror("Impossibile rilasciare il mutex main del file");
        return 1;
    }
    return 0;
}

// funzione che aggiunge un nuovo utente nella sua tabella hash
int addUtente(char* username, long pos){
    // creiamo una nuova struttura utente
    Utente* user = malloc(sizeof(Utente));
    if(user == NULL){
        fprintf(stderr,"Errore malloc per nuovo utente\n");
        return 1;
    }
    char* use = malloc(strlen(username)+1);
    if(use == NULL){
        fprintf(stderr,"Errore malloc username use\n");
        return 1;
    }
    strcpy(use,username);
    user->username = use;
    user->seek = pos;
    //aggiungiamo il nodo nella tabella hash
    if(addNode(userTable,username,(void*)user) != 0){
        fprintf(stderr,"Errore impossibile aggiungere un nuovo nodo nella tabella hash\n");
        return 1;
    }
    return 0;
}

//questa funzione scannerizza tutto il file credenziali alla ricerca dell'username selezionato se lo trova aggiunge un nuovo nodo nella struttura dati e ritorna 0 altrimenti ritorna 2
int findUtente(char* key){
    // blocchiamo i mutex necessari
    if(startReadFile(&usersem) != 0){
        fprintf(stderr,"Errore impossibile entrare in lettura nel file credenziali\n");
        return 1;
    }
    //apriamo il file credenziali
    //creiamo il file path
    char pathfile[strlen(FILECRED)+5];
    strcpy(pathfile,FILECRED);
    strcat(pathfile,".txt");
    FILE* file = fopen(pathfile,"r");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire in lettura il file credenziali\n");
        return 1;
    }
    //scandiamo il file riga per riga controllando prima se quest'ultima e' valida e poi il nome utente
    char* username = malloc(sizeof(char)*MAX_ID);
    if(username == NULL){
        fprintf(stderr,"Errore malloc username\n");
        return 1;
    }
    int temp;
    long pos = ftell(file);
    while(fscanf(file,"%d %s %*s\n",&temp,username) == 2){
        printf("scanner di una riga con %s..\n",username);
        if(temp == 1){
            // la riga corrente è valida
            if(strcmp(key,username) == 0){
                //abbiamo trovato l'utente
                if(addUtente(username,pos) != 0){
                    fprintf(stderr,"Errore impossibile aggiungere un nuovo nodo utente nella tabella hash\n");
                    free(username);
                    fclose(file);
                    return 1;
                }
                // rilasciamo il mutex
                if(endReadFile(&usersem) != 0){
                    fprintf(stderr,"Errore impossibile rilacaire il semaforo read al file credenziali\n");
                    free(username);
                    fclose(file);
                    return 1;
                }
                free(username);
                fclose(file);
                return 0;
            }
        }
        pos = ftell(file);
    }
    //non è stato trovato l'utente richiesto quindi ritorniamo valore 2
    if(endReadFile(&usersem) != 0){
        fprintf(stderr,"Errore impossibile rilascaire mutex in lettua per il file credenziali\n");
        free(username);
        fclose(file);
        return 1;
    }
    free(username);
    fclose(file);
    return 2;
}

// funzione che registra un nuovo utente
int regUtente(char* username, char* password){
    // blocchiamo il file in scrittura
    if(startWriteFile(&usersem) != 0){
        fprintf(stderr,"Errore semaforo in fase di scrittura in credenziali\n");
        return 1;
    }
    //apriamo il file in append
    char pathfile[strlen(FILECRED)+5];
    strcpy(pathfile,FILECRED);
    strcat(pathfile,".txt");
    FILE* file = fopen(pathfile,"a");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire il file in modlaità append\n");
        return 1;
    }
    int pos = ftell(file);
    //scriviamo la nuova tupla
    if(fprintf(file,"1 %s %s\n",username,password) < 0){
        perror("Errore durante la scrittura sul file credenziai");
        fclose(file);
        return 1;
    }
    fflush(stdout);
    //aggiungiamo l'utente all'interno della tabella hash
    if(addUtente(username,pos) != 0){
        fprintf(stderr,"Errore impossibile aggiungere l'utente nella tabella hash\n");
        fclose(file);
        return 1;
    }
    //rilasciamo il mutex della scrittura
    if(endWriteFile(&usersem) != 0){
        fprintf(stderr,"Errore semaforo in fase di scrittura in credenziali\n");
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

// funzione che inserisce in password la password dell'utente altrimenti ritorn 1
int findPswd(long pos, char* password){
    //bocco in lettura il file credenziali
    if(startReadFile(&usersem) != 0){
        fprintf(stderr,"Errore blocco semafor in lettura creddnziali\n");
        return 1;
    }
    // apro il file credenziali in modalità lettura
    char pathfile[strlen(FILECRED)+5];
    strcpy(pathfile,FILECRED);
    strcat(pathfile,".txt");
    FILE* file = fopen(pathfile,"r");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire il file in modlaità append\n");
        return 1;
    }
    // mi sposto al seek definito
    if (fseek(file, pos, SEEK_SET) != 0) {
        perror("Errore durante il posizionamento del puntatore del file");
        fclose(file);
        return 1;
    }
    // leggo la passowerd
    fscanf(file,"%*d %*s %s\n",password);
    //rilascio il mutex di lettura
    if(endReadFile(&usersem) != 0){
        fprintf(stderr,"Errore rilascio semaforo in lettura\n");
        return 1;
    }
    return 0;
}

// funzione che aggiunge un nodo chat all'interno della tabella hash
int addChat(char* chatid, long pos){
    // allochiamo spazio per la nuova struttura Chat
    Chat* chat = malloc(sizeof(Chat));
    if(chat == NULL){
        fprintf(stderr,"Errore malloc per nuova chat\n");
        return 1;
    }
    // allochiamo memoria per id della nuova chat
    char* id = malloc(strlen(chatid)+1);
    if(id == NULL){
        fprintf(stderr,"Errore malloc per id chat\n");
        return 1;
    }
    // inseriamo nel campo id della struttura il nome della chat
    strcpy(id,chatid);
    chat->chat_id = id;
    // inseriamo il seek all'interno della struttura
    chat->seek = pos;
    // inzializziamo la struttura dei semafori per il file chat
    if(pthread_mutex_init(&(chat->sem.main),NULL) != 0){
        perror("Errore inizializzazione mutex chat");
        return 1;
    }
    if(sem_init(&(chat->sem.readers),0,0) != 0){
        perror("Errore inizializzazione semaforo chat");
        return 1;
    }
    // aggiungiamo il nodo nella tabella hash
    if(addNode(chatTable,id,(void*)chat) != 0){
        fprintf(stderr,"Errore impossibile aggiungere un nuovo nodo nella tabella hash\n");
        return 1;
    }
    return 0;
}

// funzione che registra una nuova chat nel file chat e inserisce il nodo nella tabella hash e ne crea il file di archiviazione dei messaggi
int regChat(char* chat_id, int numpart, char part[][MAX_ID]){
    // il blocco della scrittura sul file chat è lascaito al chiamante
    //apriamo il file in modalità append
    char pathfchat[strlen(FILECHAT)+5];
    strcpy(pathfchat,FILECHAT);
    strcat(pathfchat,".txt");
    FILE* file = fopen(pathfchat,"a");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire il file in modlaità append\n");
        return 1;
    }
    // prendiamo la posizione del file corrente
    int pos = ftell(file);
    // scriviamo sul file la nuova chat inserendo id e numero dei partecipanti
    if(fprintf(file,"1 %s %d",chat_id,numpart) < 0){
        perror("Errore durante la scrittura sul file");
        fclose(file);
        return 1;
    }
    // inseriamo il nome di ogni partecipante della chat
    for(int i=0;i<numpart;i++){
        fprintf(file," %s",part[i]);
        printf("scritto %s\n",part[i]);
    }
    // aggiungiamo il carattere \n per posizionarci ad una nuova riga pronti per una nuova scrittura
    fprintf(file,"\n");
    fflush(file);
    //creiamo il nome univoco della chat utilizzando il chat_id e il nome del creatore
    char chatname[strlen(chat_id)+strlen(part[0])+1] // nome della chat univoca "chatid_creatore"
    strcpy(chatname,chat_id);
    strcat(chatname,part[0]);
    // aggiungiamo la chat all'interno della tabella hash
    if(addChat(chatname,pos) != 0){
        fprintf(stderr,"Errore impossibile aggiungere la chat nella tabella hash\n");
        fclose(file);
        return 1;
    }
    // creiamo il file della chat all'interno di una cartella prestabilita
    //creiamo il pathname
    char pathc[strlen(CHAT_FOLDER)+strlen(chatname)+1];
    strcpy(pathc,CHAT_FOLDER);
    strcat(pathc,chatname);
    if(initFile(pathc) != 0){
        fprintf(stderr,"Errore impossibile inizializzare il file chat\n");
        return 1;
    }
    // sblocco scrittura sul file chat è lasciato al chiamante
    // chiudiamo il file aperto
    fclose(file);
    // restituiamo 0 se tutto è andato a buon fine
    return 0;
}

// funzione che effettua una ricerca nel file chat su tutte le tuple composte da {chat_id,username partecipante} così da rendere unico per ogni utente il chat_id
// la funzione restituisce -2 in caso di errore , -1 in caso non è stato trovato o la posizione altrimenti
long findChat(char* chat_id,char part[][MAX_ID], int numpart){
    // il blocco della lettura sul file chat è lasciato al chiamante
    //apriamo il file chat
    char pathfile[strlen(FILECHAT)+5];
    strcpy(pathfile,FILECHAT);
    strcat(pathfile,".txt");
    FILE* file  = fopen(pathfile,"r");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire il file in modlaità lettura\n");
        return -2;
    }
    //scorriamo il file
    long pos = ftell(file);
    int temp = 0;
    char id[MAX_CHATID];
    char user[MAX_ID];
    while(fscanf(file,"%d ",&temp) == 1){
        if(temp == 1){
            // la riga corrente è valida
            fscanf(file,"%s %d",id,&temp);
            if(strcmp(id,chat_id)==0){
                // la chat ha lo stesso id
                for(int i=0;i<temp;i++){
                    fscanf(file," %s",user);
                    for(int j=0;j<numpart;j++){
                        if(strcmp(user,part[j]) == 0){
                            // abbiamo trovato il file con un partecipante
                            return pos;
                        }
                    }
                }
            } 
        }
        pos = ftell(file);
    }
    // non è stata trovata una chat con quel determinato chat_id e almeno uno dei partecipanti
    return -1;
}

// questo file apre un file chat specifico ad una determinata posizione del file CHATS  e invia tutti i messaggi al client
/*
int sendChat(long pos,Chat data,int socket){
    // creiamo il path per il file chats
    char pathfchat[strlen(FILECHAT)+5];
    strcpy(pathfchat,FILECHAT);
    strcat(pathfchat,".txt");
    // blocchiamo la lettura sul file chats
    if(startReadFile(chatsem) != 0){
        fprintf(stderr,"Errore impossibile iniziare una lettura sul file CHATS\n");
        return 1;
    }
    // apriamo il file chat in lettura
    FILE * file = fopen(pathfchat, "r");
    if(file == NULL){
        fprintf(stderr,"Errore nell'apertura del file chats\n");
        return 1;
    }
    // posizioniamo il seek al punto in cui è scritta la tupla della chat desisderata
    if(fseek(file,pos,SEEK_SET) != 0){
        perror("Errore nel posizionamento del seek nel file chat");
        fclose(file);
        return 1;
    }
    // prendiamo il chatid e l'username creatore della chat
    char creator[MAX_ID];
    char chatid[MAX_CHATID];
    if(fscanf("%*d %s %*d %s",chatid,creator) != 2){
        perror("Errore nella lettura del file chat con fscanf");
        fclose(file);
        return 1;
    }
    //creiamo la stringa per il path della chat
    char pathc[strlen(CHAT_FOLDER)+strlen(chatid)+1+strlen(creator)+5];
    strcpy(pathc,CHAT_FOLDER);
    strcat(pathc,chatid);
    strcat(pathc,"_");
    strcat(pathc,creator);
    strcat(pathc,".txt");
    //apriamo la chat di interresse
    FILE * chat = fopen(pathc,"r");
    if(chat == NULL){
        perror(stderr,"Errore nell'apertura della chat di interesse\n");
        return 1;
    }
    // inviamo tutto il file al client per stamparlo
    Messaggio mess;
    while(fscanf(chat,"%*d %s\n%s\n%s\n%s\n%s\n",mess.mittente,mess.object,mess.text,mess.data,mess.ora) == 5){
        //inviamo la struttura messaggio al client
        if(send_encrypted_data(socket,&mess,sizeof(mess),server_tx) != 0){
            fprintf(stderr,"Errore invio messaggio al client\n");
            return 1;
        }
    }
}

int writeMessage(long pos){
    // creiamo il path per il file chats
    char pathfchat[strlen(FILECHAT)+5];
    strcpy(pathfile,FILECHAT);
    strcat(pathfile,".txt");
    //blocchiamo la scrittura sul file
    if(startWriteFile())
}
*/


/* Funzione che invia al client tutte le chat che sono attualmente create per un determinato utente che lo richiede la funzione restituisce il numero di chat inviate -1 in caso di errore*/
int printChatID(char* username, int socket, unsigned char* server_tx){
    int numchat = 0;
    // costruiamo la stringa per aprire il file chat
    char pathfchat[strlen(FILECHAT)+5];
    strcpy(pathfchat,FILECHAT);
    strcat(pathfchat,".txt");
    // blocchiamo la lettura per il file chats
    if(startReadFile(&chatsem) != 0){
        fprintf(stderr,"Errore impossibile inziare a leggere il file chats\n");
        return -1;
    }
    // apriamo il file chats
    FILE* file = fopen(pathfchat,"r+"); // lettura all'inizio del file (utilizzo + poichè devo settare il seek)
    if(file == NULL){
        perror("Errore nell'aprire il file chat");
        return -1;
    }
    // leggiamo ogni riga valida controllando se nei partecipanti è indicato l'username specificato
    int valrow;
    char chat_id[MAX_CHATID];
    int numpart;
    char partecipante[MAX_ID];
    long seek;
    int i = 0; // indice utile per il controllo dei partecipanti
    int validate = 1; // intero utile per verificare se un utente è all'interno di una chat [0=presente nella chat][1=partecipanti ancora non controllati][2=non presente nella chat]
    while(fscanf(file,"%d ",&valrow) == 1){
        // esiste una nuova riga
        // controlliamo se la riga è valida altrimenti la skippiamo
        if(valrow == 1){
            // la riga è valida
            // avvisiamo il client che c'è un'altra chat da stampare
            if(send_encrypted_int(socket,1,1,server_tx) != 0){
                fprintf(stderr,"Errore impossibile inviare comando di altra riga al client\n");
                return -1;
            }
            // leggiamo chat_id e numero partecipanti
            if(fscanf(file,"%s %d",chat_id,&numpart) != 2){
                perror("Errore nella lettura di una tupla in chats");
                return -1;
            }
            // controlliamo se fra i partecipanti è presente anche username
            validate = 1;
            i = 0;
            seek = ftell(file);
            while ( validate == 1){
                // leggiamo parteciapnte
                fscanf(file," %s",partecipante);
                if(strcmp(partecipante,username) == 0){
                    // l'utente è presente nella chat
                    validate = 0;
                } else {
                    i++;
                }
                if(i>=numpart){
                    validate = 2;
                }
            }
            if(validate == 0){
                // la chat contiene username
                numchat++;
                // inviamo il chat_id al client
                if(send_encrypted_data(socket,(const unsigned char*)chat_id,sizeof(chat_id),server_tx) != 0){
                    fprintf(stderr,"Errore impossibile inviare chat_id al client\n");
                    return -1;
                }
                //inviamo il numero di partecipanti al client
                if(send_encrypted_int(socket,numpart,snprintf(NULL,0,"%d",MAX_PART),server_tx) != 0){
                    fprintf(stderr,"Errore impossibile inviare num partecipanti al client\n");
                    return -1;
                }
                // settiamo il seek nella posizione precedentemente salvata
                if(fseek(file,seek,SEEK_SET) != 0){
                    perror("Errore nel posizionamento del seek per leggere i partecipanti da inviare al client");
                    return -1;
                }
                //inviamo tutti i partecipanti tranne se stesso al client
                for(int j=0;j<numpart-1;j++){
                    fscanf(file," %s",partecipante);
                    if(strcmp(partecipante,username) != 0){
                        // inviamo il partecipante al client
                        if(send_encrypted_data(socket,(const unsigned char*)partecipante,sizeof(partecipante),server_tx) != 0){
                            fprintf(stderr,"Errore impossibile inviare nome partecipante al client\n");
                            return -1;
                        }
                    }
                }
            }
        }
        // skippiamo la riga fino alla prossima
        while(fgetc(file)!='\n');
    }
    //avvisiamo il client che non ci sono più chat da stampare
    if(send_encrypted_int(socket,0,1,server_tx) != 0){
        fprintf(stderr,"Errore impossibile inviare comando di fine chats al client\n");
        return -1;
    }
    return numchat;
}

// funzione che crea una nuova chat inserendola nel file logchats, aggiungendola alla tabella hash e creando il file di archiviazione
int createChat() {
    int resp = -1; // valore di ritorno verso il client
    int numpart; // numero di partecipanti alla nuova chat
    // ricezione del numero di partecipanti alla nuova conversazione da creare
    if(receive_encrypted_int(socket,&numpart,snprintf(NULL,0,"%d",MAX_PART),server_rx) != 0){ 
        fprintf(stderr,"Errore nel ricevere il numero di partecipanti\n");
        return 1;
    }
    printf("numero partecipanti: %d\n",numpart);
    // ricezione degli username dei partecipanti
    char partecipanti[numpart][MAX_ID];
    strcpy(partecipanti[0],username); //il creatore sarà il primo username
    int i = 1;
    while(i<numpart){
        resp = -1;
        // ricezione dell'i-esimo partecipante 
        if(receive_encrypted_data(socket,(unsigned char*)partecipanti[i],sizeof(partecipanti[i]),server_rx) != 0){
            fprintf(stderr,"Errore nella ricezione di un parteciapnte ad una nuova chat\n");
            return 1;
        }
        printf("partecipante: %s\n",partecipanti[i]);
        // controlliamo che l'utente non sia stato già inserito
        for(int j=0;j<i;j++){
            if(strcmp(partecipanti[i],partecipanti[j]) == 0){
                resp = 2;
            }
        }
        // se non è stato già inserito controlliamo l'esistenza di un utente con quell'username
        if( resp != 2){
            // cerchiamo l'utente all'interno della hashtable
            if( searchNode(userTable,partecipanti[i],compareUtente) != NULL){ 
                //user trovato nella tabella hash
                resp = 0;
            // controlliamo l'esistenza dell'utente all'interno del file credenziali
            } else if(findUtente(partecipanti[i]) == 0){
                //user trovato nel file
                resp = 0;
            } else {
                // username non trovato
                resp = 1;
            }
        } 
        if(resp == 0){
            // se l'username è accettato incementiamo il puntatore
            i++;
        }
        // inviamo la risposta al client
        if(send_encrypted_int(socket,resp,1,server_tx) != 0){
            fprintf(stderr,"Errore nell'invio della risposta al client\n");
            return 1;
        }
    }
    // ricezione del nome della chat (controllando che ogni partecipante non abbia già una chat con quel nome, altrimenti il nome va cambiato)
    resp = -1;
    char chat_id[MAX_CHATID];
    long pos;
    while(resp != 0){
        // riceviamo chat_id dal client
        if(receive_encrypted_data(socket,(unsigned char*)chat_id,sizeof(chat_id),server_rx) != 0){
            fprintf(stderr,"Errore impossibile ricevere la chat_id\n");
            return 1;
        }
        // blocchiamo la lettura sul file chat
        if(startWriteFile(&chatsem) != 0){
            fprintf(stderr,"Errore nel bloccare la scrittura sul file chat\n");
            return 1;
        }
        // controlliamo se esiste una chat con lo stesso nome e almeno uno dei partecipanti all'interno
        if((pos = findChat(chat_id,partecipanti,numpart)) != -1){
            if(pos == -2){
                fprintf(stderr,"Errore durante la ricerca nel file chat\n");
                return 1;
            }
            resp = 1;
            // sblocchiamo la lettura sul file chat
            if(endWriteFile(&chatsem) != 0){
                fprintf(stderr,"Errore nel rilascio dei semafori dopo la lettura sul file chat\n");
                return 1;
            }
        } else {
            // non è stata trovata nessuna chat
            resp = 0;
        }
        // inviamo risposta al client
        if(send_encrypted_int(socket,resp,1,server_tx) != 0){
            fprintf(stderr,"Errore impossibile inviare risposta al client\n");
            return 1;
        }
    }
    printf("chat_id: %s\n",chat_id);
    // registriamo la nuova chat inserendola nel log e nella tabella hash e creando il file di archiviazione dei messaggi
    if(regChat(chat_id, numpart,partecipanti) != 0){
        fprintf(stderr,"Errore impossibile inviare risposta al client\n");
        return 1;
    }
    // rilasciamo il mutex della scrittura
    if(endWriteFile(&chatsem) != 0){
        fprintf(stderr,"Errore impossibile sbloccare la scrittura sul file chat\n");
        return 1;
    }
    return 0;
}

// la funzione apre una chat esistente
int openChat() {
    int resp = 0;
    char chat_id[MAX_CHATID];
    if((resp = printChatID()) <= 0){
        if(resp < 0){
            fprintf(stderr,"Errore impossibile inviare tuttue le chat disponibili al client\n");
            return 1;
        }
        return 0;
    }
    // ricevi la chat id a cui vuole accedere
    resp = -1;
    while(resp == -1){
        if(receive_encrypted_data(socket, chat_id, sizeof(chat_id), server_tx) != 0){
        fprintf(stderr,"Errore impossibile ricevere la chat id da aprire\n");
        return 1;
        }
        // controlla che sia accettabile per questo username
        // verifica che esiste una chat_id con il partecipante
        // prendi chat_idcreatore
    }
    // blocca la lettura  del semaforo nella tabella hash
    // leggi tutta la chat e inviala al client

    // scrivi messaggio elimina messsaggio
}

// la funzione elimina la chat esistente
int removeChat(){
    int resp = 0;
    char chat_id[MAX_CHATID];
    if((resp = printChatID()) <= 0){
        if(resp < 0){
            fprintf(stderr,"Errore impossibile inviare tuttue le chat disponibili al client\n");
            return 1;
        }
        return 0;
    }
    // ricevi la chat id che vuole eliminare
    resp = -1;
    while(resp == -1){
        if(receive_encrypted_data(socket, chat_id, sizeof(chat_id), server_tx) != 0){
        fprintf(stderr,"Errore impossibile ricevere la chat id da aprire\n");
        return 1;
        }
        // controlla che sia accettabile per questo username
        // verifica che esiste una chat_id con il partecipante
        // prendi chat_idcreatore
    }
    // elimina la riga nel logfile
    // elimina il nodo nella tabella hash
    // elimina il file
}
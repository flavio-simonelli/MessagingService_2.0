#include "server.h" // file header in cui sono descritte tutte le firme delle funzioni e le struttre dati utilizzati dal server

struct semFile chatsem; // struttura di semafori per la concorrenza di accesso nel file logChats
struct semFile usersem; // struttura di semafori per la concorrenza di accesso nel file Credentials
Node* chatTable[MAX_TABLE]; // Tabella hash che contiene nodi Chat
Node* userTable[MAX_TABLE]; // Tabella hash che contiene nodi Utente
int serverSocket; // Descrittore della socket instanzata dal client

/* Funzione principale del Server (Partenza) */
int main(int argc, char **argv){
    // inseriamo 2 parametri: indirizzo ip e porta del server
    // controllo inserimento dei 2 parametri
    if(argc != 3){
        fprintf(stderr,"Usare: %s [IP address] (if IP=null is the same of machine) [port]\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    // inizializzazione della gestione dei segnali
    if(initSignal() != 0){
        fprintf(stderr,"errore nell'inizializzazione dei segnali\n");
        exit(EXIT_FAILURE);
    }
    // inizializzazione della socket
    if(initSocket(argv[1], argv[2]) != 0){
        fprintf(stderr,"errore nell'inizializzazione della socket\n");
        exit(EXIT_FAILURE);
    }
    // inizializzazione della libreria di crypto
    if(initCrypto() != 0){
        fprintf(stderr,"Errore nell'inizializzazione della libreria crittografica\n");
        closeServer();
        exit(EXIT_FAILURE);
    }
    // inizializzazione della base di dati
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
    }
    closeServer();
    return 0;
}


/* Funzione che chiude il server correttamente in caso di segnale */
void signalclose(){
    closeServer();
    printf("\n Il server e' stato chiuso correttamente\n");
    exit(EXIT_SUCCESS);
}

/* Funzione che contiuene tutte le chiusure indispensabili per il server prima del suo spegnimento */
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
    /* adesso SIGTERM E SIGINT avviano la funzione signalclose mentre sigpipe è stato ignorato esplicitamente con un conseguente comportamnento di default */

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

/* Questa funzione ha lo scopo di inizializzare le strutture dati "volatili" hash table e di eliminirare le righe invalide dei file archiviati (credentials e logchats) */
int initDataBase(){
    // eliminazione delle righe invalidate per il file credenziali
    if(initFile(FILECRED) != 0){
        fprintf(stderr,"Errore impossibile inizializzare il file credenziali\n");
        return 1;
    }
    // inizializziamo il mutex per la scrittura del file
    if(pthread_mutex_init(&(usersem.main),NULL) != 0){
        perror("Errore nell'inizializzazione del mutex per il file credentials");
        return 1;
    }
    // inizializziamo il semaforo per la lettura del file
    if(sem_init(&(usersem.readers),0,0) != 0){
        perror("Errore nell'inizializzazione del semaforo per i readers in file chat");
        return 1;
    }
    // eliminiamo le righe invalide per il file chat
    if(initFile(FILECHAT) != 0){
        fprintf(stderr,"Errore impossibile inizializzare il file chat\n");
        return 1;
    }
    // inizializziamo il mutex per la scrittura del file
    if(pthread_mutex_init(&(chatsem.main),NULL) != 0){
        perror("Errore nell'inizializzazione del mutex per il file chat");
        return 1;
    }
    // inizializziamo il semaforo per la lettura del file
    if(sem_init(&(chatsem.readers),0,0) != 0){
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

/* Funzione che elimina le righe invalide di un determinato file a parametro (le righe invalide sono contrassegnate con uno 0 nella prima posizione) 
(il valore di ritorno è 2 se c'è stato un errore nel "travasare" i dati fra il file temporaneo e il file originale) */
int initFile(char* nomeFile){
    // aggiungiamo ".txt" al nomefile
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
    // creazione del duplicato
    char pathtemp[strlen(pathfile)+strlen("temp")+1];
    strcpy(pathtemp,"temp");
    strcat(pathtemp,pathfile);
    FILE *dupfile = fopen(pathtemp,"w+"); //apertura in scrittura troncata 
    if(dupfile == NULL){
        fprintf(stderr,"Errore: non e' stato possibile aprire il file duplicato \n");
        fclose(originalfile);
        return 1;
    }
    // copia del file originale nel file temporaneo
    int temp;
    while((temp = fgetc(originalfile)) != EOF){
        fputc(temp,dupfile);
    }
    // spostiamo il seek del dupfile all'inizio del file
    fseek(dupfile, 0, SEEK_SET);
    // chiudiamo il file orginale per riaprlirlo troncato
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


/* Funzione principale per ogni thread creato, implementa le vere funzionalità dell'applicazione */
void *mainThread(void *clientSocket) {
    int socket = *((int *)clientSocket); //cast del descrittore della socket
    char username[MAX_ID]; // username dell'utente collegato al thread
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
    // ricezione dell'operazione di autenticazione (0 = accesso, 1 = registrazione, 2 = eliminazione account)
    int op;
    if(receive_encrypted_int(socket,&op,1,server_rx) != 0){
        fprintf(stderr,"Errore impossibile ricevere operazione di autenticazione dal client\n");
        close(socket);
        pthread_exit(NULL);
    }
    // ricezione dell'username 
    int resp = 1; // intero che corrisponde alla risposta da inviare al client
    Utente* user = NULL;
    while(resp != 0){
        // ricezione della stringa username dal client
        if(receive_encrypted_data(socket,(unsigned char*)username,MAX_ID,server_rx) != 0){
            fprintf(stderr,"Errore impossibile ricevere username dal client\n");
            close(socket);
            pthread_exit(NULL);
        }
        if(op != 1){
            // fase di accesso o eliminazione 
            // cerchiamo un nodo con lo stesso username nella tabella hash
            Node* node = NULL;
            if((node=searchNode(userTable,username,compareUtente)) != NULL){ 
                //user trovato nella tabella hash
                user = (Utente*)node->content;
                resp = 0;
            } else if(findUtente(username) == 0){ // ricerca dell'username all'interno del file credenziali
                // username trovato nel file
                // prendiamo il nodo della tabella hash riferito a quel utente
                if((node = searchNode(userTable,username,compareUtente)) != NULL){
                    user = (Utente*)node->content;
                    resp = 0;
                }
            } else {
                // username non esistente
                resp = 1;
            }
        } else {
            // fase di registrazione
            // vediamo se esiste già un utente con quell'username 
            if(searchNode(userTable,username,compareUtente) != NULL){
                // esiste un nodo con quell`username 
                resp = 2;
            } else {
                // cerchiamo nel file una persona con quell'username 
                if(findUtente(username) == 2){
                    // username ancora non utilizzato
                    resp = 0;
                } else {
                    // username già esistente
                    resp = 2;
                } 
            }
        }
        // inviamo risposta al client
        if(send_encrypted_int(socket,resp,1,server_tx) != 0){
            fprintf(stderr,"Errore nell'invio della rispsota\n");
            close(socket);
            pthread_exit(NULL);
        }
    }
    // fase password
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
        // operazione richiesta accesso o eliminazione account
        // prelevo la password dal file
        if(findPswd(user->seek,hashed_password) != 0){
            fprintf(stderr,"Errore prelievo password dal file credenziali\n");
            close(socket);
            pthread_exit(NULL);
        }
    }
    // ricezione password
    while(resp != 0){
        // riceviamo la password
        if(receive_encrypted_data(socket,(unsigned char*)password,MAX_PSWD,server_rx)!=0){
            printf("errore nella ricezione della password in chiaro dal server \n");\
            close(socket);
            pthread_exit(NULL);
        }
        if(op != 1){
            // login o eliminazione
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
        // blocchiamo la scrittura sul file credenziali
        if(startWriteFile(&usersem) != 0){
            fprintf(stderr,"Errore impossibile bloccare la scrittura sul file credenziali\n");
            close(socket);
            pthread_exit(NULL);
        }
        // eliminiamo tutte le chat che aveva creato questo utente
        if(delChatsforUser(username) != 0){
            fprintf(stderr,"Errore nell'eliminazione delle chat dell'utente\n");
            endWriteFile(&usersem);
            close(socket);
            pthread_exit(NULL);
        }
        // eliminiamo il nodo utente all'interno della hash table e dal file credenziali
        if(rmNode(userTable,username,compareUtente,rmUtente) != 0){
            fprintf(stderr,"Errore nell'eliminazione dell'utente\n");
            endWriteFile(&usersem);
            close(socket);
            pthread_exit(NULL);
        }
        // rilascio della scrittura sul file cred
        if(endWriteFile(&usersem) != 0){
            fprintf(stderr,"Errore impossibile sbloccare la scrittura sul file credenziali\n");
            close(socket);
            pthread_exit(NULL);
        }
    }

    // Fase principale
    char destinatario[MAX_ID]; // buffer che conterrà il destinatario richiesto dall'utente
    char nomechat[MAX_ID*2+1]; // buffer che conterrà il nome della chat richiesta dall'utente 
    Node* c; // nodo nella tabella hash delle chat richiesto dall'utente
    char timestamp[20];
    char object[MAX_OBJECT];
    char text[MAX_TEXT]; 
    // entriamo in un ciclo infinito
    while(1){
        op = -1; // resettiamo il valore dell'operazione a default
        // riceviamo operazione da eseguire {0 = leggi chat} {1 = scrivi messaggio} {2 = elimina messaggio}
        if(receive_encrypted_int(socket,&op,1,server_rx) != 0){
            fprintf(stderr,"Errore impossibile ricevere operazione di autenticazione dal client\n");
            close(socket);
            pthread_exit(NULL);
        }
        // richiesta destinatario
        resp = 1;
        while(resp != 0){
            //aspettiamo l'username dal client
            if(receive_encrypted_data(socket,(unsigned char*)destinatario,MAX_ID,server_rx) != 0){
                fprintf(stderr,"Errore impossibile ricevere username dal client\n");
                close(socket);
                pthread_exit(NULL);
            }
            // cerchiamo se esiste l'utente destinatario scritto dal client
            if(searchNode(userTable,destinatario,compareUtente) != NULL){
                // utente trovato nella tabella hash
                resp = 0;
            } else if(findUtente(destinatario) == 0){
                resp = 0;
            }
            //inviamo risposta al client
            if(send_encrypted_int(socket,resp,1,server_tx) != 0){
                fprintf(stderr,"Errore nell'invio della rispsota\n");
                close(socket);
                pthread_exit(NULL);
            }
        }
        // dopo aver selezionato il destinatario corretto creiamo il nome univoco della chat di riferimento
        createNameChat(username,destinatario,nomechat);
        // adesso in base all'operazione richiesta dal client {0 = leggi chat} {1 = scrivi messaggio} {2 = elimina messaggio}
        // in qualunque situazione prima vediamo se la chat esiste se no la creiamo
        resp = -1;
        // cerchiamo il file chat
        if((c = searchNode(chatTable,nomechat,compareChat)) == NULL){
            // file non è stato trovato nella hashtable
            // concorrenza in lettura sul file logchats
            if(startReadFile(&chatsem) != 0){
                fprintf(stderr,"Errore impossibile entrare in lettura nel file chat\n");
                close(socket);
                pthread_exit(NULL);
            }
            resp = findChat(nomechat);
            // concorrenza in lettura
            if(endReadFile(&chatsem) != 0){
                fprintf(stderr,"Errore impossibile lasciare lettura nel file chat\n");
                close(socket);
                pthread_exit(NULL);
            }
            if(resp != 0){
                if(resp == 2){
                    // la chat non è stata trovata e quindi dobbiamo crearla
                    // creiamo la nuova chat
                    if(regChat(nomechat) != 0){
                        fprintf(stderr,"Errore nell'invio della rispsota\n");
                        close(socket);
                        pthread_exit(NULL);
                    }
                    resp = 0;
                } else {
                    // c'è stato un errore nella funzione findchat
                    resp = 1;
                }
            }
            if(resp != 1){
                // la chat è stata trovata nel file o è stata correttamente creata quindi prendiamo il nodo corrispondente
                if((c = searchNode(chatTable,nomechat,compareChat)) == NULL){
                    // la chat probabilmente è stata eliminata durante il processo da qualche altro thread
                    resp = 1;
                } else {
                    // abbiamo il nodo corrispondente alla chat
                    // inizializziamo il file chat eliminando le righe non valide
                    // blocchiamo il file in scrittura
                    if(startWriteFile(&((Chat*)(c->content))->sem) != 0){
                        fprintf(stderr,"Errore nel blocco della scrittura sul file chat\n");
                        close(socket);
                        pthread_exit(NULL);
                    }
                    // eliminiamo le righe non valide
                    if(initChat(((Chat*)(c->content))->chat_id) != 0){
                        fprintf(stderr,"Errore nella inizializzazione di un file chat\n");
                        close(socket);
                        pthread_exit(NULL);
                    }
                    // sblocchiamo il file
                    if(endWriteFile(&((Chat*)(c->content))->sem) != 0){
                        fprintf(stderr,"Errore nello sblocco della scrittura sul file chat\n");
                        close(socket);
                        pthread_exit(NULL);
                    }
                    resp = 0;
                }
            }
        } else {
            // inizializziamo il file chat eliminando le righe non valide
            // blocchiamo il file in scrittura
            if(startWriteFile(&((Chat*)(c->content))->sem) != 0){
                fprintf(stderr,"Errore nel blocco della scrittura sul file chat\n");
                close(socket);
                pthread_exit(NULL);
            }
            // eliminiamo le righe non valide
            if(initChat(((Chat*)(c->content))->chat_id) != 0){
                fprintf(stderr,"Errore nella inizializzazione di un file chat\n");
                close(socket);
                pthread_exit(NULL);
            }
            // sblocchiamo il file
            if(endWriteFile(&((Chat*)(c->content))->sem) != 0){
                fprintf(stderr,"Errore nello sblocco della scrittura sul file chat\n");
                close(socket);
                pthread_exit(NULL);
            }
            resp = 0;
        }
        if(op == 0){
            // operazione richiesta leggi messaggi
            // blocchiamo in lettura il file
            if(resp == 0){
                if(startReadFile(&((Chat*)(c->content))->sem) != 0){
                    fprintf(stderr,"Errore nel blocco in lettura del file chat\n");
                    resp = 1;
                }
            }
            // inviamo risposta al client
            if(send_encrypted_int(socket,resp,1,server_tx) != 0){
                fprintf(stderr,"Errore nell'invio della rispsota\n");
                if(resp == 0){
                    endReadFile(&(((Chat*)(c->content))->sem));
                }
                close(socket);
                pthread_exit(NULL);
            }
            if(resp == 0){
                if(sendChat(((Chat*)(c->content))->chat_id,socket,server_tx) != 0){
                    fprintf(stderr,"Errore impossibile inviare la chat al cliente\n");
                    endReadFile(&(((Chat*)(c->content))->sem));
                    close(socket);
                    pthread_exit(NULL);
                }
                // sblocchiamo la lettura
                if(endReadFile(&(((Chat*)(c->content))->sem)) != 0){
                    fprintf(stderr,"Errore nel bloccare lettura sul file\n");
                    close(socket);
                    pthread_exit(NULL);
                }
            }
        } else if (op == 1){
            // operazione richiesya scrivi messaggio
            // blocchiamo in scrittura il file
            if(op == 0){
                if(startWriteFile(&((Chat*)(c->content))->sem) != 0){
                    fprintf(stderr,"Errore nel blocco della scrittura sul file chat\n");
                    resp = 1;
                }
            }
            // inviamo risposta al client
            if(send_encrypted_int(socket,resp,1,server_tx) != 0){
                fprintf(stderr,"Errore nell'invio della rispsota\n");
                if(resp == 0){
                    endWriteFile(&((Chat*)(c->content))->sem);
                }
                close(socket);
                pthread_exit(NULL);
            }
            if(resp == 0){
                // ricezione dell'ogetto
                if(receive_encrypted_data(socket,(unsigned char*)object,MAX_OBJECT,server_rx) != 0){
                    fprintf(stderr,"Errore impossibile ricevere ogetto del messaggio\n");
                    endWriteFile(&((Chat*)(c->content))->sem);
                    close(socket);
                    pthread_exit(NULL);
                }
                // ricezione del testo
                if(receive_encrypted_data(socket,(unsigned char*)text,MAX_TEXT,server_rx) != 0){
                    fprintf(stderr,"Errore impossibile ricevere testo del messaggio\n");
                    endWriteFile(&((Chat*)(c->content))->sem);
                    close(socket);
                    pthread_exit(NULL);
                }
                // scrittura del messaggio
                if(writeMessage(nomechat,username,object,text) != 0){
                    fprintf(stderr,"Errore impossibile scrivere il messaggio nel file chat\n");
                    resp = 1;
                } else {
                    resp = 0;
                }
                // sblocchiamo la scrittura
                if(endWriteFile(&((Chat*)(c->content))->sem) != 0){
                    fprintf(stderr,"Errore nello sblocco della scrittura sul file chat\n");
                    close(socket);
                    pthread_exit(NULL);
                }
                // inviamo la risposta al client
                if(send_encrypted_int(socket,resp,1,server_tx) != 0){
                    fprintf(stderr,"Errore nell' invio della risposta al client\n");
                    close(socket);
                    pthread_exit(NULL);
                }
            }
        } else if (op == 2){
            // operazione richiesta elimina messaggio
            // blocchiamo in scrittura il file
            if(resp == 0){
                if(startWriteFile(&((Chat*)(c->content))->sem) != 0){
                    fprintf(stderr,"Errore nel blocco della scrittura sul file chat\n");
                    resp = 1;
                }
            }
            // inviamo risposta al client
            if(send_encrypted_int(socket,resp,1,server_tx) != 0){
                fprintf(stderr,"Errore nell'invio della rispsota\n");
                if(resp == 0){
                    endWriteFile(&((Chat*)(c->content))->sem);
                }
                close(socket);
                pthread_exit(NULL);
            }
            if(resp == 0){
                // inviamo i messaggi al client
                if(sendChat(((Chat*)(c->content))->chat_id,socket,server_tx) != 0){
                    fprintf(stderr,"Errore impossibile inviare la chat al cliente\n");
                    endWriteFile(&((Chat*)(c->content))->sem);
                    close(socket);
                    pthread_exit(NULL);
                }
                // richiediamo il timestamp da eliminare
                if(receive_encrypted_data(socket,(unsigned char*)timestamp,20,server_rx) != 0){
                    fprintf(stderr,"Errore impossibile ricevere il timestamp da eliminare\n");
                    endWriteFile(&((Chat*)(c->content))->sem);
                    close(socket);
                    pthread_exit(NULL);
                }
                // eliminiamo il timestamp
                resp = invalidaMessaggio(((Chat*)(c->content))->chat_id,timestamp);
                // sblocchiamo la scrittura
                if(endWriteFile(&((Chat*)(c->content))->sem) != 0){
                    fprintf(stderr,"Errore nello sblocco della scrittura sul file chat\n");
                    close(socket);
                    pthread_exit(NULL);
                }
                // inviamo la risposta al client
                if(send_encrypted_int(socket,resp,1,server_tx) != 0){
                    fprintf(stderr,"Errore nell' invio della risposta al client\n");
                    close(socket);
                    pthread_exit(NULL);
                }
            }
        }
    }
    close(socket);
    pthread_exit(NULL);
}


/* la funzione serve per scambiare le chiavi crittografiche fra il client e il thread associato */
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


/* Hash Function */
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

// funzione per liberare una struttura chat e il file che indicava
int rmChat(void* chat){
    //cast del puntatore
    Chat* chatptr = (Chat*)chat;
    //distruggiamo il semaforo per scrivere un messaggio
    if(pthread_mutex_destroy(&(chatptr->sem.main)) != 0){
        perror("Errore nella distruzione del mutex per la scrittura di un file in fase di eliminazione");
        return 1;
    }
    if(sem_destroy(&(chatptr->sem.readers)) != 0){
        perror("Errore nella distruzione del semaforo per i lettori del file");
        return 1;
    }
    //elimininiamo la chat
    char pathchat[strlen(chatptr->chat_id)+6];
    sprintf(pathchat, "%s.txt", chatptr->chat_id);
    if(remove(pathchat) != 0){
        perror("Errore nella eliminazione del file chat");
        return 1;
    }
    //liberiamo la memoria allocata per il chat_id
    free(chatptr->chat_id);
    // invalidiamo la riga nel file logchats
    char pathfile[strlen(FILECHAT)+5];
    strcpy(pathfile,FILECHAT);
    strcat(pathfile,".txt");
    FILE* file = fopen(pathfile,"r+");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire il file logchats\n");
        return 1;
    }
    if(fseek(file,chatptr->seek,SEEK_SET) != 0){
        perror("Errore nel posizionamento del seek");
        fclose(file);
        return 1;
    }
    fprintf(file,"0");
    fclose(file);
    //liberiamo la memoria riservata alla struttura dati chat
    free(chatptr);
    return 0;
}

// funzione per liberare la memoria di una struttura dati Utente e invalidare la riga nel file crednziali (si assume che sia stata già bloccata la scrittutera sul file credenziali)
int rmUtente(void* user){
    //cast del puntatore
    Utente* userptr = (Utente*)user;
    // invalidiamo la riga nel file credenziali
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
    //liberiamo la memoria per riservata per l'username
    free(userptr->username);
    free(userptr);
    return 0;
}

// funzione per eliminare un nodo da una tabella hash
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

/* funzione che cerca un nodo all'interno della sua hashtable */
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

/* funzione che gestisce i mutex per la lettura su file */
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

/* funzione che gestisce la struttura dati semaforo */
int endReadFile(struct semFile* sem){
    //riprendiamo il gettone del semaforo che avevamo rilasciato allo start
    sem_wait(&(sem->readers));
    return 0;
}

/* funzione che gestisce la struttura dati semaforo */
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

/* funzione che gestisce la struttura dati semaforo */
int endWriteFile(struct semFile* sem){
    // rilasciamo il mutex main
    if(pthread_mutex_unlock(&(sem->main)) != 0){
        perror("Impossibile rilasciare il mutex main del file");
        return 1;
    }
    return 0;
}

/* funzione che aggiunge un nuovo utente nella sua tabella hash */
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

/* questa funzione scannerizza tutto il file credenziali alla ricerca dell'username selezionato se lo trova aggiunge un nuovo nodo nella struttura dati e ritorna 0 altrimenti ritorna 2 (il valore è 1 in caso di errore)*/
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

/* funzione che registra un nuovo utente all'interno del file credenziali e nella tabella hash (chiamando la funzione addUtente)*/
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
    //aggiungiamo l'utente all'interno della tabella hash
    if(addUtente(username,pos) != 0){
        fprintf(stderr,"Errore impossibile aggiungere l'utente nella tabella hash\n");
        fclose(file);
        return 1;
    }
    fclose(file);
    //rilasciamo il mutex della scrittura
    if(endWriteFile(&usersem) != 0){
        fprintf(stderr,"Errore semaforo in fase di scrittura in credenziali\n");
        return 1;
    }
    return 0;
}

/* funzione che inserisce in password la password dell'utente altrimenti ritorna 1 */
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

/* Funzione che aggiunge la chat all'interno della tabella hash */
int addChat(char* chatid, long pos){
    // creiamo una nuova struttura chat
    Chat* chat = malloc(sizeof(Chat));
    if(chat == NULL){
        fprintf(stderr,"Errore malloc per nuova chat\n");
        return 1;
    }
    // creiamo una nuova stringa chat_id
    char* id = malloc(strlen(chatid)+1);
    if(id == NULL){
        fprintf(stderr,"Errore malloc per id chat\n");
        return 1;
    }
    // copiamo la stringa nello spazio dedicato
    strcpy(id,chatid);
    // impostiamo il nodo chat
    chat->chat_id = id;
    chat->seek = pos;
    // inizializziamo la struttura per la concorrenza nella chat
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

/* funzione che registra una nuova chat nel file chat e inserisce il nodo nella tabella hash */
int regChat(char* chat_id){
    // blocchiamo il file in scrittura
    if(startWriteFile(&chatsem) != 0){
        fprintf(stderr,"Errore semaforo in fase di scrittura in chats\n");
        return 1;
    }
    // apriamo il file in append
    char pathfile[strlen(FILECHAT)+5];
    strcpy(pathfile,FILECHAT);
    strcat(pathfile,".txt");
    FILE* file = fopen(pathfile,"a");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire il file in modlaità append\n");
        return 1;
    }
    int pos = ftell(file);
    //scriviamo la nuova tupla
    if(fprintf(file,"1 %s\n",chat_id) < 0){
        perror("Errore durante la scrittura sul file");
        fclose(file);
        return 1;
    }
    //aggiungiamo la chat all'interno della tabella hash
    if(addChat(chat_id,pos) != 0){
        fprintf(stderr,"Errore impossibile aggiungere la chat nella tabella hash\n");
        fclose(file);
        return 1;
    }
    //rilasciamo il mutex della scrittura
    if(endWriteFile(&chatsem) != 0){
        fprintf(stderr,"Errore semaforo in fase di scrittura in chat\n");
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

/* Questa funzione ha lo scopo di trovare una determinata chat all'interno del file chat, se esite crea il nodo all'interno dela tabella hash */
int findChat(char* key){
    //apriamo il file logchat
    //creiamo il file path
    char pathfile[strlen(FILECHAT)+5];
    strcpy(pathfile,FILECHAT);
    strcat(pathfile,".txt");
    FILE* file = fopen(pathfile,"r");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire in lettura il file chat\n");
        return 1;
    }
    //scandiamo il file riga per riga controllando prima se quest'ultima e' valida e poi il nome della chat
    char* nomechat = malloc(sizeof(char)*MAX_ID*2+2); // buffer che contiene il nome della chat
    if(nomechat == NULL){
        fprintf(stderr,"Errore malloc nomechat\n");
        return 1;
    }
    int temp; // buffer che contiene il bit validate
    long pos = ftell(file); // posizione della riga nel file
    while(fscanf(file,"%d %s\n",&temp,nomechat) == 2){
        if(temp == 1){
            // la riga corrente è valida
            if(strcmp(key,nomechat) == 0){
                // la chat esiste in archivio quindi la carichiamo nella tabella hash
                if(addChat(nomechat,pos) != 0){
                    fprintf(stderr,"Errore impossibile aggiungere un nuovo nodo chat nella tabella hash\n");
                    free(nomechat);
                    fclose(file);
                    return 1;
                }
                free(nomechat);
                fclose(file);
                return 0;
            }
        }
        pos = ftell(file);
    }
    //non è stata trovata la chat richiesta quindi ritorniamo il valore 2
    free(nomechat);
    fclose(file);
    return 2;
}

/* Questa funzione ha lo scopo di combinare i partecipanti alla chat per restituire il nome */
void createNameChat(char str1[], char str2[], char risultato[]) {
    // Confronta le stringhe lessicograficamente
    if (strcmp(str1, str2) < 0) {
        strcpy(risultato, str1);  // Copia la prima stringa
        strcat(risultato, "-");    // Aggiungi un trattino
        strcat(risultato, str2);   // Aggiungi la seconda stringa
    } else {
        strcpy(risultato, str2);  // Copia la seconda stringa
        strcat(risultato, "-");    // Aggiungi un trattino
        strcat(risultato, str1);   // Aggiungi la prima stringa
    }
}

// Funzione per impostare a 0 il bit di validità del messaggio corrispondente al timestamp nel file specificato
int invalidaMessaggio(char* nomeFile, char* timestamp) {
    // Aggiungi l'estensione ".txt" al nome del file
    char nomeFileTxt[strlen(nomeFile)+5];
    sprintf(nomeFileTxt, "%s.txt", nomeFile);

    // Apri il file in modalità lettura e scrittura
    FILE* file = fopen(nomeFileTxt, "r+");
    if (file == NULL) {
        perror("Errore nell'apertura del file");
        return 1; // Restituisci 1 se si verifica un errore
    }

    // Variabili per leggere il file riga per riga e identificare il messaggio corrispondente al timestamp
    char buffer[MAX_TEXT+1];
    char tempTimestamp[22];
    long posizioneInizioMessaggio = -1; // Posizione inizio del messaggio nel file

    // Leggi il file riga per riga
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        // Se trovi una riga che inizia con '1' 
        if (buffer[0] == '1') {
            // Leggi il timestamp del messaggio
            fgets(tempTimestamp, sizeof(tempTimestamp), file);

            // Rimuovi il carattere di nuova riga alla fine del timestamp
            tempTimestamp[strcspn(tempTimestamp, "\n")] = '\0';

            // Se il timestamp corrisponde a quello specificato
            if (strcmp(tempTimestamp, timestamp) == 0) {
                // Memorizza la posizione inizio del messaggio nel file
                posizioneInizioMessaggio = ftell(file) - sizeof(tempTimestamp);
                break;
            }
        }
    }

    // Se non hai trovato nessun messaggio con il timestamp specificato
    if (posizioneInizioMessaggio == -1) {
        fclose(file);
        return 1; // Restituisci 1 se non trovi il messaggio
    }

    // Imposta il bit di validità a 0 (il carattere '0') nella posizione inizio del messaggio nel file
    fseek(file, posizioneInizioMessaggio, SEEK_SET);
    fputc('0', file); // Imposta il bit di validità a 0

    // Chiudi il file
    fclose(file);

    return 0; // Restituisci 0 se tutto è andato bene
}

// Funzione per ottenere il timestamp corrente come stringa
char* getCurrentTimestamp() {
    time_t rawtime;
    struct tm* timeinfo;
    char* timestamp = malloc(20 * sizeof(char)); // Allocazione di memoria per il timestamp
    if (timestamp == NULL) {
        perror("Errore nell'allocazione di memoria");
        exit(1);
    }
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", timeinfo); // Formato del timestamp: AAAA-MM-GG HH:MM:SS
    return timestamp;
}

// Funzione per scrivere un messaggio nel file specificato !!!!!!!!!!!!!! DA modificareeeee
int writeMessage(char* nomeFile, char* username, char* object, char* text) {
    // Aggiungi l'estensione ".txt" al nome del file
    char nomeFileTxt[strlen(nomeFile)+5];
    sprintf(nomeFileTxt, "%s.txt", nomeFile);

    // Apri il file in modalità append
    FILE* file = fopen(nomeFileTxt, "a");
    if (file == NULL) {
        perror("Errore nell'apertura del file");
        return 1;
    }

    // Scrivi il messaggio nel file con il formato richiesto
    fprintf(file, "1\n"); // Flag per indicare un nuovo messaggio
    fprintf(file, "%s\n", getCurrentTimestamp()); // Timestamp corrente
    fprintf(file, "%s\n", username);
    fprintf(file, "%s\n", object);
    fprintf(file, "%s\n", text);

    // Chiudi il file
    fclose(file);
    return 0;
}

// Funzione per leggere e inviare i messaggi da un file
int sendChat(const char *filename, int socket, const unsigned char *tx_key) {
    // Aggiungi l'estensione ".txt" al nome del file
    char nomeFileTxt[strlen(filename)+5];
    sprintf(nomeFileTxt, "%s.txt", filename);
    FILE *file = fopen(nomeFileTxt, "r");
    if (file == NULL) {
        perror("Errore nell'apertura del file");
        return 1;
    }
    char line[MAX_TEXT+1];
    char message[MAX_ID+20+MAX_TEXT+MAX_OBJECT+5];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Leggi il bit "validate"
        if (line[0] == '1'){
            // il messaggio è valido
            if(send_encrypted_int(socket,1,1,tx_key) != 0){
                fprintf(stderr,"Errore impossibile inviare bit di controllo al client\n");
                fclose(file);
                return 1;
            }
            // leggiamo il timestamp
            fgets(line, sizeof(line), file);
            // Rimozione del carattere di \n dalla fine della stringa
            line[strcspn(line, "\n")] = '\0';
            strcpy(message,line);
            strcat(message," ");
            // leggiamo il mittente
            fgets(line, sizeof(line), file);
            strcat(message,line);
            // leggiamo l'ogetto
            fgets(line, sizeof(line), file);
            strcat(message,line);
            // leggiamo il testo
            fgets(line, sizeof(line), file);
            strcat(message,line);
            //inviamo la stringa al client
            if(send_encrypted_data(socket,(const unsigned char*)message,sizeof(message),tx_key) != 0){
                fprintf(stderr,"Errore impossibile inviare messaggio al client\n");
                fclose(file);
                return 1;
            }
        } else {
            // non considero il messaggio successivo
            fgets(line,sizeof(line),file);
            fgets(line,sizeof(line),file);
            fgets(line,sizeof(line),file);
            fgets(line,sizeof(line),file);
        }
    }
    // Segnala al client che non ci sono altri messaggi da inviare
    if(send_encrypted_int(socket,0,1,tx_key) != 0){
        fprintf(stderr,"Errore impossibile inviare bit di controllo al client\n");
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

// questa funzione serve per eliminare ogni messaggio invalidato da una chat
int initChat(char* nomeFile){
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
    // riscrive nel file originale solo i messaggi validi
    char line[MAX_TEXT+1];
    // leggo la prima riga
    while (fgets(line,sizeof(line),dupfile) != NULL) {
        // Verifica il primo carattere
        if (line[0] == '1') { // Se il primo carattere è '1'
            // copia la riga nel file originale
            fprintf(originalfile,"%s",line); // bit validate
            fgets(line,sizeof(line),dupfile); // timestamp
            fprintf(originalfile,"%s",line);
            fgets(line,sizeof(line),dupfile); // mittente
            fprintf(originalfile,"%s",line);
            fgets(line,sizeof(line),dupfile); // ogetto
            fprintf(originalfile,"%s",line);
            fgets(line,sizeof(line),dupfile); // testo
            fprintf(originalfile,"%s",line);
        } else {
            // il messaggio non è valido e lo saltiamo
            fgets(line,sizeof(line),dupfile);
            fgets(line,sizeof(line),dupfile);
            fgets(line,sizeof(line),dupfile);
            fgets(line,sizeof(line),dupfile);
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

/* Funzione che serve ad eliminare ogni chat esistente di un determinato utente */
int delChatsforUser(char* user){
    // blocchiamo in scrittura il file chat
    if(startWriteFile(&chatsem) != 0){
        fprintf(stderr,"Errore impossibile bloccare la scrittura sul file credenziali\n");
        return 1;
    }
    // apriamo il file credenziali
    char pathfile[strlen(FILECRED)+5];
    strcpy(pathfile,FILECRED);
    strcat(pathfile,".txt");
    FILE* filecred = fopen(pathfile,"r+");
    if(filecred == NULL){
        fprintf(stderr,"Errore impossibile aprire il file in modlaità append\n");
        endWriteFile(&chatsem);
        return 1;
    }
    int validate = 0;
    char dest[MAX_ID];
    char chatID[MAX_ID*2+3];
    while (fscanf(filecred,"%d %s %*s",&validate,dest) == 2) {
        if( validate == 1){
            createNameChat(dest,user,chatID);
            // cerchiamo il file chat
            if(findChat(chatID) == 0){
                // la chat esisteva e quindi va eliminata
                if(rmNode(chatTable,chatID,compareChat,rmChat) != 0){
                    fprintf(stderr,"Errore impossibile eliminare la chat\n");
                    endWriteFile(&chatsem);
                    return 1;
                }
            }
        }
    }
    // sblocchiamo la scrittura sul file chat
    if(endWriteFile(&chatsem) != 0){
        fprintf(stderr,"Errore impossibile sbloccare la scrittura sul file chat\n");
        return 1;
    }
    return 0;
}
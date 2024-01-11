#include "server.h"

int serverSocket; // descrittore della socket lato server
HashTable* tableUser;
pthread_mutex_t writecredential;
// Lista delle chat
Listachats chatlist;


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
    if(initCredential() != 0){
        fprintf(stderr,"Errore: impossibile inizializzare il file contenente le credenziali utente\n");
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


/* La funzione inizializza il mutex per scrivere sul file credenziali e inizializza questo file cioè fa una copia di quello precedentemente salvato e lo riscrive sull'originale mantenendo solo le righe valide */
int initCredential(){
        // creazione di un mutex per la scrittura sul file credenziali
    if( pthread_mutex_init(&(writecredential),NULL) != 0){
        perror("Errore nell'inizializzazione del mutex per il file credenziali");
        return 1;
    }
    // creazione della struttura dati per contenere gli username e i seek dell'utente
    tableUser = initializeHashTable();
    if(tableUser == NULL){
        fprintf(stderr,"Errore, impossibile creare la hash table\n");
        return 1;
    }
    //apertura file originale
    FILE *originalfile = fopen(CREDPATH,"r+"); //apertura del file in RDWR all'inizio
    if(originalfile == NULL){
        //il file con le credenziali non esiste e lo crea
        originalfile = fopen(CREDPATH,"w");
        fclose(originalfile);
        return 0;
    }
    //crea il duplicato
    char pathtemp[strlen(CREDPATH)+strlen("temp")+1];
    strcpy(pathtemp,"temp");
    strcat(pathtemp,CREDPATH);
    FILE *dupfile = fopen(pathtemp,"w+"); //come prima ma viene anche troncato
    if(dupfile == NULL){
        fprintf(stderr,"Errore: non e' stato possibile aprire il file duplicato delle credenziali \n");
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
    originalfile = fopen(CREDPATH,"w+");
    if(originalfile == NULL){
        fprintf(stderr,"Errore: non e' stato possibile aprire il file contenente le credenziali \n");
        fclose(dupfile);
        return 1;
    }
    // riscrive nel file originale solo le credenziali valide
    Utente user;
    char buffer[1+1+MAX_ID+1+ENC_PSWD+1+1];
    // Leggi ogni riga dal file duplicato
    while (fgets(buffer, sizeof(buffer), dupfile) != NULL) {
        // Verifica il primo carattere
        if (buffer[0] == '1') { // Se il primo carattere è '1'
            // Prendi il seek del file nel punto in cui è scritto l'utente
            user.pos = ftell(originalfile);
            // copia la riga nel file originale
            fputs(buffer, originalfile);
            // prendi l'username dalla stringa appena letta
            if(sscanf(&(buffer[2]),"%s[^ ]",user.username) != 1){ // -------------------------------------------------------
                fprintf(stderr,"Errore: impossibile leggere username");
                return 1;
            }
            //inserisci l'utente nella tabella hash
            if(insertIntoHashTable(tableUser,user) != 0){
                fprintf(stderr,"Errore: impossibile inserire elemento nella tabella hash");
                return 1;
            }
        }
        // Altrimenti, se è '0', la riga viene saltata
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

// Funzione per inizializzare la struttura leggendo i file nella cartella
int inizializzaChatFiles(FileChat** listaChat) {
    //inizializziamo la lista chat
    &chatlist = malloc(sizeof(Listachats));
    if(&chatlist == NULL){
        perror("Errore impossibile inizializzare la lista delle chat");
        return 1;
    }
    chatlist.head = NULL;
    if(pthread_mutex_init(&(chatlist.add)) != 0){
        perror("Impossibile inizializzare mutex lista chat");
        return 1;
    }
    DIR* dir;
    struct dirent* entry;

    // Verifica se la directory "Chats" esiste, altrimenti creala
    if (mkdir(CHAT_FOLDER, 0777) == -1 && errno != EEXIST) {
        perror("Errore nella creazione della directory delle chat");
        return 1;
    }

    // Apri la directory delle chat
    dir = opendir(CHAT_FOLDER);
    if (dir == NULL) {
        perror("Errore nell'apertura della directory delle chat");
        return 1;
    }

    // Leggi ogni file nella directory
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            // Costruisci il nome del file
            char nomeFile[strlen(entry->d_name) + 1];
            strcpy(nomeFile, entry->d_name);

            // Crea e aggiungi la nuova chat alla lista
            addChat(nomeFile);
        }
    }

    // Chiudi la directory
    closedir(dir);

    return 0;
}

// Funzione per aggiungere una nuova chat alla lista
int addChat(const char* nomeChat) {
    // Alloca memoria per la nuova chat
    FileChat* newChat = malloc(sizeof(FileChat));
    // Verifica se l'allocazione di memoria è avvenuta con successo
    if (newChat == NULL) {
        perror("Errore nell'allocazione di memoria per la nuova chat");
        return -1;  // Indica un errore
    }
    // Costruisci il nome del file
    sprintf(newChat->chat, "%s/%s", CHAT_FOLDER, nomeChat);
    // Inizializza il mutex della nuova chat
    if (pthread_mutex_init(&newChat->write, NULL) != 0) {
        perror("Errore nell'inizializzazione del mutex per la nuova chat");
        free(newChat);  // Libera la memoria allocata
        return -1;  // Indica un errore
    }
    // Imposta il puntatore al prossimo elemento a NULL
    newChat->next = NULL;
    // Aggiungi la nuova chat alla lista
    //blocchiamo il mutex scrittura nella lista
    if(pthread_mutex_lock(&(chatlist.add)) != 0){
        perror("Errore impossibile bloccare mutex add chatlist");
        return 1;
    }
    //aggiungiamo il nodo alla fine della lista
    if(chatlist.head == NULL){
        chatlist.head = newChat;
    } else {
        FileChat* temp = chatlist.head;
        while(temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = newChat;
    }
    return 0;  // Indica il successo
}

// Funzione per trovare una chat nella lista dato il nome del file
FileChat* findChat(const char* nomeChat) {
    FileChat* current = chatlist.head;
    // Cerca la chat nella lista
    while (current != NULL) {
        if (strcmp(current->chat, nomeChat) == 0) {
            // La chat è stata trovata: restituisci un puntatore alla struttura
            return current;
        }
        current = current->next;
    }
    // La chat non è stata trovata
    return NULL;
}


// Funzione per rimuovere una chat dalla lista e deallocare la memoria
int rimuoviChat(const char* nomeChat) {
    FileChat* prev = NULL;
    FileChat* current = chatlist.head;
    // Cerca la chat nella lista
    while (current != NULL) {
        if (strcmp(current->chat, nomeChat) == 0) {
            // Trovato: rimuovi la chat dalla lista
            if (prev != NULL) {
                prev->next = current->next;
            } else {
                *listaChat = current->next;
            }
            // Chiudi e distruggi il mutex
            pthread_mutex_destroy(&current->write);
            // Libera la memoria della chat
            free(current);
            return 0;  // Indica il successo
        }
        prev = current;
        current = current->next;
    }
    // La chat non è stata trovata
    return -1;  // Indica che la chat non è stata trovata
}



// Inizializza la tabella hash
HashTable* initializeHashTable() {
    HashTable* table = (HashTable*)malloc(TABLE_SIZE * sizeof(HashTable));
    for (int i = 0; i < TABLE_SIZE; i++) {
        // per correttezza inizializziamo a null tutti i puntatori alle teste delle liste
        table[i].head = NULL;
        //inizializziamo il muetx per la scrittura sulla lista
        if(pthread_mutex_init(&(table[i].mutex),NULL) != 0){
            perror("Errore nell'inizializzazione dei mutex nella tabella hash");
            return NULL;
        }
    }
    return table;
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

// funzione per registrare l'utente nel file credenziali e nella struttra dati scelta
int regUser(Utente user, char* hashpassword){
    //apriamo il file credenziali
    FILE* file = fopen(CREDPATH,"a");
    if(file == NULL){
        fprintf(stderr,"Impossibile aprire il file credenziali\n");
        return 1;
    }
    // prendiamo il mutex per scrivere sul file
    if(pthread_mutex_lock(&writecredential) != 0){
        perror("Errore: imposibile acquisire il mutex per scrivere sul file credenziali");
        fclose(file);
        return 1;
    }
    // inseriamo seek nel nuovo nodo
    user.pos = ftell(file);
    // scriviamo il nuovo utente sul file
    fprintf(file,"%d %s %s\n",1,user.username,hashpassword);

    //rilasciamo il mutex per il file 
    if(pthread_mutex_unlock(&writecredential) != 0){
        perror("Errore: imposibile rilasciare il mutex per scrivere sul file credenziali");
        fclose(file);
        return 1;
    }
    // inseriamo il nuovo nodo nella tabella
    if(insertIntoHashTable(tableUser,user) != 0){
        fprintf(stderr,"Errore: impossibile inserire il nuovo utente nella tabella hash\n");
        fclose(file);
        return 1;
    }

    fclose(file);
    return 0;
}

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



// Funzione hash basata sull'username per l'indicizzazione delle strutture utenti
// funzione di esempio (se avessi utilizzato semplicemente le lettere dell'alfabeto sarei stato bloccato a 21 liste)
unsigned int hashFunction(const char* username) {
    unsigned int hash = 0;
    for (int i = 0; username[i] != '\0'; i++) {
        hash = hash * 31 + username[i];
    }
    return hash % TABLE_SIZE;
}

// Inserisce un elemento nella tabella hash
int insertIntoHashTable(HashTable* table, Utente data) {
    //creazione di un nuovo nodo utente
    Utente *newUser = malloc(sizeof(Utente));
    if(newUser == NULL){
        perror("Errore malloc creazione nuono utente");
        return 1;
    }
    //copia dei dati
    strcpy(newUser->username,data.username);
    newUser->pos=data.pos;
    //troviamo in quale lista deve essere inserito
    unsigned int index = hashFunction(newUser->username);
    //blocchiamo il mutex per la lista
    if(pthread_mutex_lock(&(table[index].mutex)) != 0){
        perror("Errore impossibile bloccare il mutex per scrivere nella lista");
        return 1;
    }
    // inserimento in testa alla lista
    newUser->next = table[index].head;
    table[index].head = newUser;
    //sblocchiamo il mutex
    if(pthread_mutex_unlock(&(table[index].mutex)) != 0){
        perror("Errore impossibile sbloccare il mutex per scrivere nella lista");
        return 1;
    }

    return 0;
}

// Cerca un elemento nella tabella hash [DA MODIFICARE]
Utente* searchInHashTable(HashTable* table, const char* username) {
    unsigned int index = hashFunction(username);
    Utente* current = table[index].head;

    while (current != NULL) {
        if (strcmp(current->username, username) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL; // Elemento non trovato
}

// Funzione che rimuove un elemento dalla tabella hash
int removeFromHashTable(HashTable* table, const char* username) {
    unsigned int index = hashFunction(username);
    Utente* current = table[index].head;
    Utente* prev = NULL;

    //blocco del mutex per la lista
    if(pthread_mutex_lock(&(table[index].mutex)) != 0){
        perror("Errore: imposibile acquisire il mutex per eliminare il nodo dalla lista");
        return 1;
    }

    while (current != NULL) {
        if (strcmp(current->username, username) == 0) {
            if (prev == NULL) {
                // Il nodo da rimuovere è il primo nella lista
                table[index].head = current->next;
            } else {
                // Il nodo da rimuovere non è il primo nella lista
                prev->next = current->next;
            }
            
            // sblocco il mutex
            if(pthread_mutex_unlock(&(table[index].mutex)) != 0){
                perror("Errore: impossibile restituire il mutex per eliminare il nodo dalla lista");
                return 1;
            }

            free(current); // Libera la memoria della struttura Utente
            return 0;
        }
        prev = current;
        current = current->next;
    }

    return 1;
}




int deleteUser(Utente user){   
    //apriamo il file in scrittura
    FILE* file = fopen(CREDPATH,"r+");
    if(file == NULL){
        fprintf(stderr,"Errore impossibile aprire il file credenziali\n");
        return 1;
    }
    //spostiamo il seek
    if(fseek(file,user.pos,SEEK_SET) != 0){
        perror("Errore posizionamento nel file durante l'eliminazione");
        fclose(file);
        return 1;
    }
    //blocchiamo la scrittura sul file
    if(pthread_mutex_lock(&writecredential) != 0){
        perror("Errore: imposibile acquisire il mutex per scrivere sul file credenziali");
        fclose(file);
        return 1;
    }
    //mettiamo ad 1 il bit validate
    if(fprintf(file,"0") < 0){
        fprintf(stderr,"Errore nella scrittura sul file credenziali\n");
        return 1;
    }
    //rilasciamo la scrittura sul file
    if(pthread_mutex_unlock(&writecredential) != 0){
        perror("Errore: imposibile rilasciare il mutex per scrivere sul file credenziali");
        fclose(file);
        return 1;
    }
    //eliminiamo il nodo dalla tabella hash
    if(removeFromHashTable(tableUser,user.username) != 0){
        fprintf(stderr,"Errore impossibile rimuovere il nodo dalla tabella hash\n");
        return 1;
    }

    return 0;

}

// funzione che serve per creare il file della chat univoco per ogni coppia di utenti
int findNameChat(char* chatName, char* mittente, char* destinatario){
    if(strcmp(mittente,destinatario) >= 0){
        strcpy(CHAT_FOLDER);
        strcat("/");
        strcat(chatName,mittente);
        strcat(chatName,"_");
        strcat(chatName,destinatario);
        strcat(chatname,".txt");
    } else {
        strcpy(CHAT_FOLDER);
        strcpy("/");
        strcat(chatName,destinatario);
        strcat(chatName,"_");
        strcat(chatName,mittente);
        strcat(chatname,".txt");
    }
    return 0;
}

// Funzione per ottenere un puntatore a una chat dato il nome del file
FileChat* getChatByFileName(const char* nomeFile) {
    for (int i = 0; i < MAX_CHAT_FILES; ++i) {
        if (strcmp(chatFiles[i].nomeFile, nomeFile) == 0) {
            return &chatFiles[i];
        }
    }
    fprintf(stderr, "Errore: chat non trovata per il file %s\n", nomeFile);
    return NULL;
}

// questa funzione crea una cartella per quel determinato utente e lo
int writeMessage(char* mittente, Messaggio message){
    // creiamo il nome del file dalla chat fra mittente e destinatario
    char chatName[strlen(CHAT_FOLDER)+1+strlen(mittente)+1+strlen(message.destinatario)+strlen(".txt")+1];
    findNameChat(chatName, mittente, message.destinatario);

    FileChat* file;
    //cerchiamo il file nella lista
    if((file=findChat(chatName)) == NULL){
        // se non esiste lo creiamo
        if(addChat(chatName) != 0){
        fprintf(stderr,"Errore impossibile aggiungere una nuova chat alla lista\n");
        return 1;
        }
        //prendiamo il suo puntatore
        if((file=findChat(chatname)) == NULL){
            fprintf(stderr,"Errore impossibile creare un nuovo file chat\n");
            return 1;
        }
    }
    //blocchiamo la scrittura sul file
    if(pthread_mutex_lock(&(file->write)) != 0){
        perror("Errore impossibile bloccare il muutex di scrittura sul file");
        return 1;
    }
    //scriviamo il messaggio alla fine del file con il time stamp


}
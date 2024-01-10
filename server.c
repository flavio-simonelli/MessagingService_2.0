#include "server.h"

int serverSocket; // descrittore della socket lato server
HashTable* tableUser;
pthread_mutex_t writecredential;


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
    if(originalfile == NULL){
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
    return 0;
}

// Inizializza la tabella hash
HashTable* initializeHashTable() {
    HashTable* table = (HashTable*)malloc(TABLE_SIZE * sizeof(HashTable));
    for (int i = 0; i < TABLE_SIZE; i++) {
        // per correttezza inizializziamo a null tutti i puntatori alle teste delle liste
        table[i].head = NULL;
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
        // Cifra la password in chiaro
        if (crypto_pwhash_str(hashed_password, password, strlen(password), crypto_pwhash_OPSLIMIT_SENSITIVE, crypto_pwhash_MEMLIMIT_SENSITIVE) != 0) {
            perror("error to hash password ");
            return 1;
        }
        if(op == 1){
            //l'utente vuole registrarsi
            if( regUser(*utente, hashed_password) != 0){
                fprintf(stderr,"Errore: impossibile registrare il nuovo utente\n");
                return 1;
            }
            instr_server = 0;
        } else {
            //l'utente vuole loggarsi o eliminare l'account
            
            if ( crypto_pwhash_str_verify(pswdsaved, hashed_password, strlen(hashed_password)) != 0) {
                // Password sbagliata
                instr_server = 1; // messaggio al client "password errata" 
            } else {
                instr_server = 0; //messaggio al client "password corretta"
            }
        }
        //sblocco la memoria riservata alla password in chiaro
        sodium_munlock(password, sizeof(password));
        // Invia risultato al client
        send_encrypted_int(socket,instr_server,server_tx);
    }
    return 0;
}

/* Questa funzione serve a chiudere correttamente tutte le componenti del server*/
void closeServer(){
    close(serverSocket);
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
    // inserimento in testa alla lista
    newUser->next = table[index].head;
    table[index].head = newUser;
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

// funzione per registrare l'utente nel file credenziali e nella struttra dati scelta
int regUser(Utente user, char* hashpassword){
    //apriamo il file credenziali
    FILE* file = fopen(CREDPATH,"a");
    if(file == NULL){
        fprintf(stderr,"Impossibile aprire il file credenziali\n");
        return 1;
    }
    // inseriamo seek nel nuovo nodo
    user.pos = ftell(file);
    // scriviamo il nuovo utente sul file
    fprintf(file,"%d %s %s\n",1,user.username,hashpassword);
    // inseriamo il nuovo nodo nella tabella
    if(insertIntoHashTable(tableUser,user) != 0){
        fprintf(stderr,"Errore: impossibile inserire il nuovo utente nella tabella hash\n");
        fclose(file);
        return 1;
    }
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

//int remUser(Utente user){   
//}
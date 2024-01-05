#include "server.h"

int serverSocket; // descrittore della socket lato server

int main(int argc, char **argv){
    // inseriamo 2 parametri: indirizzo ip e porta del server
    // controllo inserimento dei 2 parametri
    if(argc != 3){
        fprintf(stderr,"Usare: %s [IP address] (if IP=null is the same of machine) [port]\n",argv[0]);
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

    // inizializzazione del file credenziali
    if(initCredential() != 0){
        fprintf(stderr,"Errore: impossibile inizializzare il file contenente le credenziali utente\n");
        closeServer();
        exit(EXIT_FAILURE);
    }

    //accettazione di nuove connessioni con i client attraverso threads separati
    int clientSocket; // descrittore della connessione con il client
    while(1){
        //mettiamo la socket in stato di accettazione
        struct sockaddr_in client_addr; // inizializziamo una struttura per l'indirizzo del client
        socklen_t client_addr_len = sizeof(client_addr);
        clientSocket = accept(serverSocket, (struct sockaddr*)&client_addr, &client_addr_len);
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

//funzione main dei thread
void *mainThread(void *clientSocket) {
    int socket = *((int *)clientSocket); //cast del descrittore della socket
    pthread_exit(NULL);
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
            fprintf(stderr,"Errore, l'indirizzo IP non e' valido.\n");
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

/* La funzione inizializza il file credenziali, cioè fa una copia di quello precedentemente salvato e lo riscrive sull'originale mantenendo solo le righe valide (MAncano i Semafori)*/
int initCredential(){
    // crea una copia del file credenziali
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
    char buffer[500]; //da modificare perchè solo di prova --------------------------------------------------------------------------
    // Leggi ogni riga dal file duplicato
    while (fgets(buffer, sizeof(buffer), dupfile) != NULL) {
        // Verifica il primo carattere
        if (buffer[0] == '1') {
            // Se il primo carattere è '1', copia la riga nel file originale
            fputs(buffer, originalfile);
        }
        // Altrimenti, se è '0', la riga viene saltata
    }

    // Chiude entrambi i file
    fclose(dupfile);
    fclose(originalfile);

    return 0;
}
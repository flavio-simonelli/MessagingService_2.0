#include "client.h"

//variabili glocbali per lo scambio di messaggi criptati con il server
unsigned char client_pk[crypto_kx_PUBLICKEYBYTES], client_sk[crypto_kx_SECRETKEYBYTES];
unsigned char client_rx[crypto_kx_SESSIONKEYBYTES], client_tx[crypto_kx_SESSIONKEYBYTES];
unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];

int sock; // descrittore della socket a cui è collegato il client

int main(int argc, char** argv){
    // controllo se sono stati inseriti tutti i parametri
    if(argc != 3){
        fprintf(stderr,"Errore: usare ./client [IP Server] (null usa quello della macchina) [Porta]\n");
        exit(EXIT_FAILURE);
    }
    // inizializzazione della socket
    if(initSocket(argv[1],argv[2]) != 0){
        fprintf(stderr,"Errore: non è stata possibile inizializzare la connessione con il server\n");
        exit(EXIT_FAILURE);
    }
    // Impostazione del gestore del segnale SIGINT
    signal(SIGINT, sigint_handler);
    // inizializzazione della libreria di crittografia
    if(initCrypto() != 0){
        fprintf(stderr,"Errore: impossibile inizializzare la libreria di crittografia \n");
        exit(EXIT_FAILURE);
    }
    // richiesta operazione da eseguire
    int op;
    char *option[] = {"accesso", "registrazione", "eliminazione account"};
    if( operationrequire(&op,option,3) != 0){ //funzione che chiede all'utente di scegliere un'opzione fra quelle elencate
        printf("errore nella richiesta di un'operazione pre autenticazione \n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    //invio operazione da eseguire al server
    if( send_encrypted_int(sock,op,1,client_tx) != 0){
        printf("errore invio operazioen pre autenticazione \n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    //fase di autenticazione
    int resp = -1;
    char* username;
    while( resp != 0){
            // allocazione di memoria per l'userrname
        if((username = (char*)malloc(MAX_ID * sizeof(char))) == NULL){
            perror("error to malloc for username");
            exit(EXIT_FAILURE);
        }
        //richiesta username
        if(stringrequire(username,MAX_ID,"username",4)!=0){
            printf("errore nella richiesta del'username \n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        if(send_encrypted_data(sock,(const unsigned char*)username, MAX_ID, client_tx) != 0){
            printf("errore invio userrname \n");
            close(sock);
            exit(EXIT_FAILURE);
        }
    	//aspettiamo la risposta del server
        if(receive_encrypted_int(sock,&resp,1,client_rx) != 0){
            fprintf(stderr,"Errore impossibile ricevere la risposta dal server\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        if(resp == 2){
            printf("Username esistente!\n");
        } else if(resp == 1){
            printf("Username insesistente!\n");
        }
        free(username);
    }
    printf("Username accettato!\n");
    resp = -1;
    //richiesta password utente
    char *password; // buffer temporaneo per la password in chiaro
    do{
        // allocazione di memoria per la password in chiaro
        if((password = (char*)malloc(MAX_PSWD * sizeof(char))) == NULL){
            perror("error to malloc for password");
            exit(EXIT_FAILURE);
        }
        //richiesta password
        if(stringrequire(password,MAX_PSWD,"password",8) != 0){
            printf("errore nella richiesta della password \n");
            return 1;
        }
        // invio della password in chiaro
        if(send_encrypted_data(sock,(const unsigned char*)password,MAX_PSWD,client_tx)!=0){
            printf("errore nell'invio della password verso il server \n");
            return 1;
        }
        // attesa della risposta da parte del server
        if(receive_encrypted_int(sock,&resp,1,client_rx) !=0){
            printf("errore nella ricezione della rispodta del client \n");
        }
        if(resp == 1){ // password errata in caso di accesso o eliminazione dall'account
            printf("password errata, per favore inserisci la password corretta \n");
        }
        free(password); // deallochiamo memoria per password in chiaro
    }while(resp != 0);

    if(op==0){
        printf("Accesso avvenuto con successo! \nBentornato ! \n");
    } else if( op == 1){
        printf("Registrazione avvenuta con successo! \nBenvenuto ! \n");
    } else{
        printf("Eliminazione dell'account avvenuta correttamente! \nArrivederci ! \n");
        close(sock);
        exit(EXIT_SUCCESS);
    }

    // fase principale
    char* object;
    char* text;
    char timestamp[20];
    while(1){
        option[0] = "Leggi chat";
        option[1] = "Scrivi messaggio";
        option[2] = "Elimina messaggio";
        op = -1;
        if( operationrequire(&op,option,3) != 0){ //funzione che chiede all'utente di scegliere un'opzione fra quelle elencate
            printf("errore nella richiesta di un'operazione fase principasle \n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        //invio operazione da eseguire al server
        if( send_encrypted_int(sock,op,1,client_tx) != 0){
            printf("errore invio operazioen fase principale \n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        //invio destinatario
        resp = -1;
        while( resp != 0){
            // allocazione di memoria per l'userrname
            if((username = (char*)malloc(MAX_ID * sizeof(char))) == NULL){
                perror("error to malloc for username");
                exit(EXIT_FAILURE);
            }
            //richiesta username
            if(stringrequire(username,MAX_ID,"destinatario",4)!=0){
                printf("errore nella richiesta del destinatario \n");
                close(sock);
                exit(EXIT_FAILURE);
            }
            if(send_encrypted_data(sock,(const unsigned char*)username, MAX_ID, client_tx) != 0){
                printf("errore invio destinatario \n");
                close(sock);
                exit(EXIT_FAILURE);
            }
            //aspettiamo la risposta del server
            if(receive_encrypted_int(sock,&resp,1,client_rx) != 0){
                fprintf(stderr,"Errore impossibile ricevere la risposta dal server\n");
                close(sock);
                exit(EXIT_FAILURE);
            }
            if( resp == 1){
                printf("Destinatario inesistente!\n");
            } else {
                printf("destinatario trovato!\n");
            }
            free(username);
        }
        resp = -1;
        //aspettiamo la risposta del server
        if(receive_encrypted_int(sock,&resp,1,client_rx) != 0){
            fprintf(stderr,"Errore impossibile ricevere la risposta dal server\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
        if(resp == 0){
            if( op == 0){
                // riceviamo la chat
                printChat();
            } else if( op == 1){
                // scriviamo il messaggio
                // allocazione di memoria per l'userrname
                if((object = (char*)malloc(MAX_OBJECT * sizeof(char))) == NULL){
                    perror("error to malloc for object");
                    exit(EXIT_FAILURE);
                }
                // richiediamo ogetto
                if(stringrequire(object,MAX_OBJECT,"oggetto",4)!=0){
                    printf("errore nella richiesta dell'oggetto \n");
                    close(sock);
                    exit(EXIT_FAILURE);
                }
                // inviamo ogetto
                if(send_encrypted_data(sock,(const unsigned char*)object, MAX_OBJECT, client_tx) != 0){
                    printf("errore invio object \n");
                    close(sock);
                    exit(EXIT_FAILURE);
                }
                free(object);
                // allocazione di memoria per l'userrname
                if((text = (char*)malloc(MAX_TEXT * sizeof(char))) == NULL){
                    perror("error to malloc for text");
                    exit(EXIT_FAILURE);
                }
                // richiediamo testo
                if(stringrequire(text,MAX_TEXT,"testo",4)!=0){
                    printf("errore nella richiesta del testo \n");
                    close(sock);
                    exit(EXIT_FAILURE);
                }
                // inviamo testo
                if(send_encrypted_data(sock,(const unsigned char*)text, MAX_TEXT, client_tx) != 0){
                    printf("errore invio testo \n");
                    close(sock);
                    exit(EXIT_FAILURE);
                }
                free(text);
                // aspettiamo risposta dal server
                if(receive_encrypted_int(sock,&resp,1,client_rx) != 0){
                    fprintf(stderr,"Errore impossibile ricevere la risposta dal server\n");
                    close(sock);
                    exit(EXIT_FAILURE);
                }
                if( resp == 0){
                    printf("Messaggio scritto\n");
                } else {
                    printf("Impossibile scrivere il messaggio\n");
                }
            } else if( op == 2){
                // eliminiamo messaggio
                printChat();
                // richiediamo il timestamp
                if(getTimestamp(timestamp) != 0){
                    fprintf(stderr,"Errore impossibile ricevere dall'utente il timestamp\n");
                    close(sock);
                    exit(EXIT_FAILURE);
                }
                // inviamo il timestamp da eliminare
                if(send_encrypted_data(sock,(const unsigned char*)timestamp,sizeof(timestamp),client_tx) != 0){
                    fprintf(stderr,"Errore impossibile inviare il timestamp al server\n");
                    close(sock);
                    exit(EXIT_FAILURE);
                }
                //aspettiamo la risposta del server
                if(receive_encrypted_int(sock,&resp,1,client_rx) != 0){
                    fprintf(stderr,"Errore impossibile ricevere la risposta dal server\n");
                    close(sock);
                    exit(EXIT_FAILURE);
                }
                if( resp == 0){
                    printf("Eliminazione avvenuta con succcesso\n");
                } else {
                    printf("Messaggio non trovato, eliminazione impossibile\n");
                }
            }
        }
    }
    return 0;
}

// Funzione per gestire il segnale SIGINT (Ctrl+C)
void sigint_handler() {
    close(sock);
    printf("\nArrivederci!\n");
    exit(EXIT_SUCCESS);
}

int authentication(char* user){ // ricordati che nel server il mutex deve essere preso prima del controllo nella liosta degli utenti perchè così evito che nello stesso momento si registrano con lo stesso nome
    //richiesta di un nome utente
    char* username; // buffer temporaneo per username inserito dall'utente
    int validate = -1; // "instruzione lato server" operatore che comunica lo stato del server al client
    do{
        //Allocazione temporanea di memoria per l'username
        if((username = (char*)malloc(MAX_ID * sizeof(char))) == NULL){
            perror("error to malloc for username");
            exit(EXIT_FAILURE);
        }
        //richiesta dell'username
        if(stringrequire(username,MAX_ID,"username",4)!=0){
            printf("errore nella richiesta del'username \n");
            return 1;
        }
        //invio dell'username al server
        if(send_encrypted_data(sock,(const unsigned char *)username,MAX_ID,client_tx)!=0){
            printf("errore nell'invio dell'username al server \n");
            return 1;
        }
        // attesa della risposta da parte del server
        if(receive_encrypted_int(sock,&validate,1,client_rx) !=0){
            printf("errore nella ricezione della rispodta del client \n");
        }
        if(validate == 0){ //il server  conferma la ricezione dell'username
            strcpy(user,username); // copiamo l'username nel buffer definitivo
            printf("username accettato! \n");
        } else if(validate == 1){ // errore in caso di registrazione 
            printf("username già in uso! per favore inseriscine uno diverso\n");
        } else if(validate == 2){ // errore in caso di accesso o eliminazione dell'account
            printf("username inesistente! per favore inseriscine uno corretto\n");
        }
        //liberiamo memoria allocata dinamicamente
        free(username);
    }while(validate!=0);

    //richiesta password utente
    char *password; // buffer temporaneo per la password in chiaro
    validate = -1; // reset al valore di default della risposta lato server
    do{
        // allocazione di memoria per la password in chiaro
        if((password = (char*)malloc(MAX_PSWD * sizeof(char))) == NULL){
            perror("error to malloc for password");
            exit(EXIT_FAILURE);
        }

        //richiesta password
        if(stringrequire(password,MAX_PSWD,"password",8) != 0){
            printf("errore nella richiesta della password \n");
            return 1;
        }

        // invio della password in chiaro
        if(send_encrypted_data(sock,(const unsigned char*)password,MAX_PSWD,client_tx)!=0){
            printf("errore nell'invio della password verso il server \n");
            return 1;
        }
        // attesa della risposta da parte del server
        if(receive_encrypted_int(sock,&validate,1,client_rx) !=0){
            printf("errore nella ricezione della rispodta del client \n");
        }
        if(validate == 1){ // password errata in caso di accesso o eliminazione dall'account
            printf("password errata, per favore inserisci la password corretta \n");
        }
        free(password); // deallochiamo memoria per password in chiaro
    }while(validate != 0);

    return 0;
}

int initSocket(char* ipAddress, char* portstring){

    int functret; // intero utilizzato per controllare il valore di ritorno delle funzioni chiamate
    // controllo della porta
    if((functret = portValidate(portstring)) != 0){
        if(functret == 1){
            fprintf(stderr,"Errore, la porta %s specificata non e' valida.\n",portstring);
            return 1;
        } else if(functret == 2){
            fprintf(stderr,"Errore, la porta %s specificata e' minore di 5001.\n",portstring);
            return 1;
        } else if(functret == 3){
            fprintf(stderr,"Errore, la porta %s specificata e' maggiore di 65535.\n",portstring);
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

    // Inizializzazione del descrittore della socket
    sock = socket(AF_INET,SOCK_STREAM,0);
    if( sock <= 0){
        perror("Errore nella creazione della socket");
        return 1;
    }

    // Binding sock
    struct sockaddr_in addr_server;
    memset(&addr_server, 0, sizeof(addr_server));  
    addr_server.sin_family = AF_INET;
    addr_server.sin_port = htons(port);

    if( functret != 0){
        //l'utente ha inserito null
        addr_server.sin_addr.s_addr = INADDR_ANY;
    } else {
        // Conversione indirizzo IP da formato testuale a binario
        if (inet_pton(AF_INET, ipAddress, &(addr_server.sin_addr)) <= 0) {
            perror("Errore nel convertire l'indirizzo IP");
            close(sock);
            return 1;
        }
    }

    // Connessione al server
    if (connect(sock, (struct sockaddr*)&addr_server, sizeof(addr_server)) < 0) {
        perror("Errore nella connessione al server");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connessione con il Server stabilita! \n");

    return 0;
}

// funzione che inizializza la libreria libsodium per la criptazione dei dati sulla sock
int initCrypto(){
    //inizializzazione della libreria sodium
    if (sodium_init() < 0) {
        perror("problemi inizializzazione libreria libsodium");
        return 1;
    }
    // Generazione delle chiavi lato client
    crypto_kx_keypair(client_pk, client_sk);
    // Ricezione della chiave pubblica del server
    if(receive_data(server_pk,crypto_kx_PUBLICKEYBYTES,sock)!=0){
        printf("errore nella ricezione del server_pk \n");
        return 1;
    }
    //Invio della chiave pubblica al server
    if(send_data(client_pk,crypto_kx_PUBLICKEYBYTES,sock)!=0){
        printf("errore nell'invio del client_pk \n");
        return 1;
    }
    // creazione della sessione di criptazione
    if (crypto_kx_client_session_keys(client_rx, client_tx,client_pk, client_sk, server_pk) != 0) {
        printf("Errore chiave pubblica del server, riavviare l'applicazione \n");
        return 1;
    }
    printf("la connessione ora è sicura\n");
    return 0;
}

// queste due fuznioni sono state copiate dal file server quindi si potrebbe creare un file unico per queste due cose
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

/* Funzione per stampare una chat */
int printChat(){
    int resp = -1;
    char message[MAX_ID+20+MAX_TEXT+MAX_OBJECT+5];
    if(receive_encrypted_int(sock,&resp,1,client_rx) != 0){
        fprintf(stderr,"Errore nel ricevere il bit di controllo dal server\n");
        return 1;
    }
    while(resp == 1){
        // ricevi il messaggio
        if(receive_encrypted_data(sock,(unsigned char*)message,sizeof(message),client_rx) != 0){
            fprintf(stderr,"Errore nel ricevere il messaggio dal server\n");
            return 1;
        }
        // stampa messaggio
        printf("%s",message);
        // ricevi bit di controllo dal client
        if(receive_encrypted_int(sock,&resp,1,client_rx) != 0){
            fprintf(stderr,"Errore nel ricevere il bit di controllo dal server\n");
            return 1;
        }   
    }
    return 0;
}
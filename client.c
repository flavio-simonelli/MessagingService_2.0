#include "client.h"

int sock;

int main(int argc, char** argv){
    if(argc != 3){
        fprintf(stderr,"Errore: usare ./client [IP Server] (null usa quello della macchina) [Porta]\n");
        exit(EXIT_FAILURE);
    }

    if(initSocket(argv[1],argv[2]) != 0){
        fprintf(stderr,"Errore: non è stata possibile inizializzare la connessione con il server\n");
        exit(EXIT_FAILURE);
    }

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
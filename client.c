#include "client.h"

//variabili glocbali per lo scambio di messaggi criptati con il server
unsigned char client_pk[crypto_kx_PUBLICKEYBYTES], client_sk[crypto_kx_SECRETKEYBYTES];
unsigned char client_rx[crypto_kx_SESSIONKEYBYTES], client_tx[crypto_kx_SESSIONKEYBYTES];
unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];

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

    if(initCrypto() != 0){
        fprintf(stderr,"Errore: impossibile inizializzare la libreria di crittografia \n");
        exit(EXIT_FAILURE);
    }

    //richiesta operazione da eseguire
    

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


// parte funzionante della socket in windows
/*
#include <winsock2.h>
#include <stdio.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsaData;
    SOCKET Socket;
    SOCKADDR_IN SockAddr;
    int errorCode;

    errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (errorCode != 0) {
        printf("Errore nella inizializzazione di Winsock: %d\n", errorCode);
        return 1;
    }

    Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (Socket == INVALID_SOCKET) {
        printf("Errore nella creazione del socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    SockAddr.sin_port = htons(8080);
    SockAddr.sin_family = AF_INET;
    InetPtonA(AF_INET, "127.0.0.1", &(SockAddr.sin_addr.s_addr));

    errorCode = connect(Socket, (SOCKADDR*)(&SockAddr), sizeof(SockAddr));
    if (errorCode == SOCKET_ERROR) {
        printf("Errore nella connessione al server: %d\n", WSAGetLastError());
        closesocket(Socket);
        WSACleanup();
        return 1;
    }

    printf("Connesso al server!\n");

    closesocket(Socket);
    WSACleanup();

    return 0;
}
*/
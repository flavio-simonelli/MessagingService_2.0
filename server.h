#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <ctype.h>
#include <sodium.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>


#include "transfertsocket.h"

#define CREDPATH "credentials.txt"
#define BACKLOG 10
#define TABLE_SIZE 1000 //numero di esempio
#define MAX_ID 20
#define MAX_PSWD 20
#define ENC_PSWD crypto_pwhash_STRBYTES
#define MAX_OBJECT 20
#define MAX_TEXT 200

#define fflush(stdin) while(getchar() != '\n')

int initSocket (char *ipAddress, char *portstring);

int portValidate(const char *string);

int ipValidate(const char *ipAddress);

void closeServer();

int initCrypto();

int initCredential();

//funzione main dei thread
void *mainThread(void *clientSocket);

int initSignal();

void signalclose();

typedef struct Utente{
    char username[MAX_ID];
    long pos;
    struct Utente* next;
} Utente;

typedef struct {
    Utente* head;
} HashTable;

typedef struct {
    char dest[MAX_ID];
    char ogetto[MAX_OBJECT];
    char text[MAX_TEXT];
} Messaggio;


// Funzione hash basata sull'username
unsigned int hashFunction(const char* username);

// Inizializza la tabella hash
HashTable* initializeHashTable();

// Inserisce un elemento nella tabella hash
int insertIntoHashTable(HashTable* table, Utente data);

// Cerca un elemento nella tabella hash
Utente* searchInHashTable(HashTable* table, const char* username);

// Rimuove un elemento dalla tabella hash
void removeFromHashTable(HashTable* table, const char* username);

// funzione per scambiare le chiavi fra thread server e client
int key_exchange(unsigned char* server_pk, unsigned char* server_sk, unsigned char* server_rx, unsigned char* server_tx, unsigned char* client_pk, int socket);

int authentication_client(char* user, int op, unsigned char* server_rx,unsigned char* server_tx,int socket);
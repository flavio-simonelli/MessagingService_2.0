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

typedef struct {
    char username[MAX_ID];
    long pos;
    struct Utente* next;
} Utente;

typedef struct {
    struct Utente* head;
} HashTable;

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

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
#include <time.h>
#include <dirent.h>
#include <errno.h>

// file inclusi
#include "transfertsocket.h"

// VARIABILI D'AMBIENTE
#define CREDPATH "credentials.txt"
#define BACKLOG 10
#define TABLE_SIZE 1000 //numero di esempio
#define MAX_ID 20
#define MAX_PSWD 20
#define ENC_PSWD crypto_pwhash_STRBYTES
#define MAX_OBJECT 20
#define MAX_TEXT 200
#define CHAT_FOLDER "Chats"


#define fflush(stdin) while(getchar() != '\n')



// STRUTTURE DATI UTILIZZATE 
typedef struct Utente{
    char username[MAX_ID];
    long pos;
    struct Utente* next;
} Utente;

typedef struct {
    Utente* head;
    pthread_mutex_t mutex;
} HashTable;

typedef struct {
    char destinatario[MAX_ID];
    char ogetto[MAX_OBJECT];
    char text[MAX_TEXT];
} Messaggio;

typedef struct FileChat{
    char chat[5+1+MAX_ID+1+MAX_ID+4+1];
    pthread_mutex_t write;
    struct FileChat* next;
} FileChat;

typedef struct{
    FileChat* head;
    pthread_mutex_t add;
} Listachats;




// FUNZIONI INIZIALIZZAZIONE PROCESSO PRINCIPALE

int initSocket (char *ipAddress, char *portstring);

int portValidate(const char *string);

int ipValidate(const char *ipAddress);

int initCrypto();

int initCredential();

int initSignal();

void signalclose();

HashTable* initializeHashTable();

void closeServer();



// FUNZIONI DEI THREADS

int key_exchange(unsigned char* server_pk, unsigned char* server_sk, unsigned char* server_rx, unsigned char* server_tx, unsigned char* client_pk, int socket);
//parte struttura dati utenti (hashtable)
unsigned int hashFunction(const char* username);

int insertIntoHashTable(HashTable* table, Utente data);

Utente* searchInHashTable(HashTable* table, const char* username);

int removeFromHashTable(HashTable* table, const char* username);
//parte autenticazione
int authentication_client(char* user, int op, unsigned char* server_rx,unsigned char* server_tx,int socket);

char* PswdSaved(Utente user);

int regUser(Utente user, char* hashpassword);

int deleteUser(Utente user);

void *mainThread(void *clientSocket);
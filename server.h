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

#define BACKLOG 10
//massimi caratteri
#define MAX_ID 20
#define MAX_PSWD 20
#define ENC_PSWD crypto_pwhash_STRBYTES
#define MAX_OBJECT 20
#define MAX_TEXT 200
#define MAX_ID_CHAT 10
//database
#define FILECRED "credentials"
#define FILECHAT "chats"
#define USER_HASH_SIZE 100
#define CHAT_HASH_SIZE 50

#define fflush(stdin) while(getchar() != '\n')



// STRUTTURE DATI UTILIZZATE 
typedef struct Utente{
    char username[MAX_ID];
    long pos;
    pthread_mutex_t modify;
    struct Utente* next;
} Utente;

typedef struct Chat{
    char id_chat[MAX_ID_CHAT];
    long pos;
    pthread_mutex_t write;
    pthread_mutex_t modify;
    struct Chat* next;
} Chat;

typedef struct HashNode{
    void* head;
    pthread_mutex_t modify;
}HashNode;

typedef struct {
    int id_message;
    char object[MAX_OBJECT];
    char text[MAX_TEXT];
    char data[11];
    char ora[6];
}Messaggio;




// FUNZIONI INIZIALIZZAZIONE PROCESSO PRINCIPALE

int initSocket (char *ipAddress, char *portstring);

int portValidate(const char *string);

int ipValidate(const char *ipAddress);

int initCrypto();

int initSignal();

void signalclose();

void closeServer();



// FUNZIONI DEI THREADS

int key_exchange(unsigned char* server_pk, unsigned char* server_sk, unsigned char* server_rx, unsigned char* server_tx, unsigned char* client_pk, int socket);
//parte struttura dati utenti (hashtable)

int initDataBase();

unsigned int hash_function(char *str, int hash_size);

int compareUtente(const void *key, const void *node);

int compareChat(const void *key, const void *node);

int addNode(HashNode *hashTable, char *key, void *data, int hash_size, size_t structSize);

void *findNode(HashNode *hashTable, char *key, int hash_size, int (*compare)(const void *, const void *));

int deleteNode(HashNode *hashTable, char *key, int hash_size, int (*compare)(const void *, const void *));

int initFile(char* nomeFile);

int writeChat(char* id_chat, int num_part, char** part);

int writeCredential(char* username, char* password);

//parte autenticazione
int authentication_client(char* user, int op, unsigned char* server_rx,unsigned char* server_tx,int socket);

char* PswdSaved(Utente user);



void *mainThread(void *clientSocket);

// gestione chat
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

// campi massimi
#define MAX_ID 10
#define MAX_CHATID 10
#define MAX_TEXT 200
#define MAX_OBJECT 20
#define MAX_PSWD 20

// nomi file
#define FILECHAT "logChats"
#define FILECRED "logCred"

//massimo numero di righe delle tabelle
#define MAX_TABLE 100


typedef struct{
    long seek;
    char* username;
} Utente;

typedef struct{
    long seek;
    char* chat_id;
    pthread_mutex_t wmess;
} Chat;

typedef struct{
    char* mittente;
    char* destinatario;
    char* object;
    char* text;
    char* timestamp;
} Messaggio;

typedef struct Node{
    void* content;
    pthread_mutex_t modify;
    struct Node* next;
} Node;

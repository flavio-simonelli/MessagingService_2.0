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
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>


// file inclusi
#include "transfertsocket.h"

// campi massimi
#define MAX_ID 10
#define MAX_CHATID 10
#define MAX_TEXT 200
#define MAX_OBJECT 20
#define MAX_PSWD 20
#define MAX_ENCPSWD crypto_pwhash_STRBYTES
#define MAX_PART 10 //numero massimo di partecipanti

#define BACKLOG 10

// nomi file
#define FILECHAT "logChats"
#define FILECRED "logCred"
#define CHAT_FOLDER "Chats/"

//massimo numero di righe delle tabelle
#define MAX_TABLE 100

struct semFile{
    pthread_mutex_t main;
    sem_t readers;
};

typedef struct{
    long seek;
    char* username;
} Utente;

typedef struct{
    long seek;
    char* chat_id;
    struct semFile sem;
} Chat;

typedef struct{
    char mittente[MAX_ID]; //nome di chi ha scritto quel messaggio
    char object[MAX_OBJECT]; //ogetto del testo
    char text[MAX_TEXT]; //testo
    char data[9]; //giorno di invio
    char ora[6]; //orario di invio
} Messaggio;

typedef struct Node{
    void* content;
    pthread_mutex_t modify;
    struct Node* next;
} Node;


// Funzioni di inizializzazione
int initSignal();
int initSocket(char* ipAddress, char* portstring);
int initCrypto();
int initDataBase();
int initFile(char* nomeFile);

// Funzioni di validazione
int portValidate(const char* string);
int ipValidate(const char* ipAddress);

// Funzioni di gestione dei segnali
void signalclose();
void closeServer();

// Funzioni per lo scambio di chiavi crittografiche
int key_exchange(unsigned char* server_pk, unsigned char* server_sk, unsigned char* server_rx, unsigned char* server_tx, unsigned char* client_pk, int socket);

// Funzioni dei thread
void* mainThread(void* clientSocket);

// Funzioni per la gestione delle liste
unsigned int hash_function(char* key);
int compareChat(const void* a, const char* key);
int compareUtente(const void* a, const char* key);
int rmChat(void* chat);
int rmUtente(void* user);
int rmNode(Node** table, char* key, int (*compare)(const void*, const char*), int (*remove)(void *));
Node* searchNode(Node** table, char* key, int (*compare)(const void*, const char*));
int addNode(Node** table, char* key, void* data);

// Funzioni per la gestione dei file e dei semafori
int startReadFile(struct semFile* sem);
int endReadFile(struct semFile* sem);
int startWriteFile(struct semFile* sem);
int endWriteFile(struct semFile* sem);

// Funzione che aggiunge un nuovo utente nella sua tabella hash
int addUtente(char *username, long pos);

// Funzione che scannerizza il file delle credenziali alla ricerca dell'utente
int findUtente(char *key);

// Funzione che registra un nuovo utente
int regUtente(char *username, char *password);

// funzione che preleva la password dal file credenziali
int findPswd(long pos, char* password);

// Funzione che aggiunge un nodo chat alla tabella hash
// Parametri:
// - chatid: puntatore alla stringa che rappresenta l'ID della chat
// - pos: posizione nel file chat
// Restituisce 0 in caso di successo, 1 in caso di errore durante l'allocazione di memoria
int addChat(char* chatid, long pos);

// Funzione che registra una nuova chat nel file chat e inserisce il nodo corrispondente nella tabella hash
// Parametri:
// - chat_id: puntatore alla stringa che rappresenta l'ID della chat
// - numpart: numero di partecipanti alla chat
// - part: array di puntatori alle stringhe che rappresentano i nomi dei partecipanti
// Restituisce 0 in caso di successo, 1 in caso di errore durante l'apertura del file o l'allocazione di memoria
int regChat(char* chat_id, int numpart, char part[][MAX_ID]);

// Funzione che trova una chat nel file chat con un determinato chat_id e partecipanti specificati
// Parametri:
// - chat_id: puntatore alla stringa che rappresenta l'ID della chat
// - part: array di puntatori alle stringhe che rappresentano i nomi dei partecipanti
// - numpart: numero di partecipanti alla chat
// Restituisce la posizione nel file se la chat viene trovata, -1 se non viene trovata, -2 in caso di errore
long findChat(char* chat_id, char part[][MAX_ID], int numpart);


int newChat();

int printfChat(char* username, int socket, unsigned char* server_tx);
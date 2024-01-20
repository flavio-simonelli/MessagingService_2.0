#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sodium.h>
#include <signal.h>

#include "transfertsocket.h"
#include "inputuser.h"

#define MAX_ID 10
#define MAX_CHATID 10
#define MAX_TEXT 200
#define MAX_OBJECT 20
#define MAX_PSWD 20
#define MAX_ENCPSWD crypto_pwhash_STRBYTES
#define MAX_PART 10 //numero massimo di partecipanti

typedef struct {
    char dest[MAX_ID];
    char ogetto[MAX_OBJECT];
    char text[MAX_TEXT];
} Messaggio;

// il concetto che ogni coppia di utenti ha il suo file, quando un utente vuole vedere la chat si apre questa dalla fine e si legge tipo 200 caratteri poi il client puo chiedere di anadare su giu o uscire dalla chat o scrivere un messsaggio o eliminare un messaggio

int portValidate(const char *string);

int ipValidate(const char *ipAddress) ;

int initSocket(char* ipAddress, char* portstring); // questa fuznione è da cambiare per windows

int initCrypto();

int authentication(char* user);
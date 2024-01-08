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

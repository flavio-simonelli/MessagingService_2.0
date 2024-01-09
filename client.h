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

#define MAX_ID 20
#define MAX_PSWD 20

int portValidate(const char *string);

int ipValidate(const char *ipAddress) ;

int initSocket(char* ipAddress, char* portstring); // questa fuznione è da cambiare per windows

int initCrypto();

int authentication(char* user);
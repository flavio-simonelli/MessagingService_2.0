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

int portValidate(const char *string);

int ipValidate(const char *ipAddress) ;

int initSocket(char* ipAddress, char* portstring);


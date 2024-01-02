#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <ctype.h>
#include <sodium.h>

int initSocket (char *ipAddress, char *portstring);

int portValidate(const char *string);

int ipValidate(const char *ipAddress);

void closeServer();

int initCrypto();
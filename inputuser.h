#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define fflush(stdin) while(getchar() != '\n')

int operationrequire(int* num,char** options, int numopt);

int stringrequire(char* string, size_t size, char* ogetto, int minchar);

int intrequire(int *num, int max, char *oggetto);
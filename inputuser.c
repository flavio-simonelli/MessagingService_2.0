/* File che contiene tutte le funzioni utili per richiedere un input da tastiera */
#include "inputuser.h"

// ricchiesta di una operazione cablata nel software
int operationrequire(int* num,char** options, int numopt){
    // contiamo il numero di cifre possibili che l'utente può scrivere nel terminale
    int counter = 0; //counter delle cifre posizionali scrivibili
    int size = numopt;
    if(size==0){
        // se non ci sono opzioni da scegliere
        printf("non è disponibile nessuna opzione \n");
        return 1; 
    }
    while(size!=0){ // finche il numero è diverso da 0
        size /=10; // inseriamo in size il risultato della sua divisione per 10
        counter ++; // aumentiamo di una cifra posizionale
    }
    // creiamo un buffer di input
    char input[counter+2]; //aggiungiamo più due per lo \n o overflow input e l'altro per  \0
    //stampa della richiesta per l'utente
    printf("scegli un'operazione: ");
    for(int i=0; i<numopt;i++){
        printf("| [%d]%s |",i,options[i]);
    }
    printf("\t");
    bool convalidate = true;
    char *endptr;
    do{
        convalidate = true;
        if(fgets(input,sizeof(input),stdin)==NULL){
            perror("error to fgets");
            return 1;
        }
        if((strlen(input)==sizeof(input)-1) && input[strlen(input)-1]!='\n'){ // controlliamo se sono stati scritti più caratteri del necessario
            fflush(stdin);
            convalidate = false;
        }
        if(strcmp(input,"\n")==0){ // controlliamo se non è stato scritto nessun carattere
            convalidate = false;
            printf("per favore inserisci un numero valido \n");
        } else {
            // Rimuovi il carattere di nuova riga
            input[strcspn(input, "\n")] = '\0';
        }
        errno = 0;
        *num = strtol(input,&endptr,10);
        if ((errno == ERANGE) || (*endptr != '\0') || (*num < 0) || (*num >= numopt)) { // errore se non sono stati scritti tutti numeri, se il numero è negativo o maggiore della massima scelta 
        // Errore di conversione
            convalidate = false;
            printf("per favore inserisci un numero valido \n");
        }      
    }while(convalidate == false);

    return 0; //return del numero scelto
}

// Funzione per richiedere una stringa da input utente (Parametri: buffer per l'input, size buffer, stringa oggetto richiesto, min caratteri)
int stringrequire(char* string, size_t size, char* ogetto, int minchar){ // string deve essere MAX_ID e MAX_PSWD e di conseguenza anche size
    // buffer temporaneo per allocazione della stringa di input
    char * temp;
    bool c; // operatore per uscire dal ciclo do while
    
    do{
        // allocazione memoria dinamica per la stringa temp
        if ( (temp = (char*)malloc(sizeof(char)*(size+1))) == NULL ){ // allochiamo un char in più per verificare che l'utente abbia inserito un numero <= di caratteri rispetto a size
            perror(" Errore nell'allocazione della memoria per stringa in input");
            return 1;
        }
        // richiesta di input
        printf("inserisci %s (max %ld caratteri, min %d caratteri): \t",ogetto,size-1,minchar); // -1 perchè un char è per il terminatore di stringa

        c = true; // impostiamo di default l'operatore per uscire dal ciclo

        if(fgets(temp,size+1,stdin) == NULL) { c = false; } // leggiamo lenghtmax caratteri dallo stdin
        if(strcmp(temp,"\n")==0){ c = false; } // se la stringa è vuota ripetiamo il ciclo
        if( (strlen(temp) == size) && (temp[size-1] != '\n')) { // l'utente ha scritto troppi caratteri
            printf("troppo lunga! \n");
            c = false; 
            fflush(stdin);
        }
        if(strlen(temp) < (size_t)(minchar+1)) { //+1 perchè c'è il carattere \n
            printf("troppo corta! \n");
            c = false;
        }
    } while(c == false);

    // Rimuove il carattere di nuova riga
    temp[strcspn(temp, "\n")] = '\0';
    // Copia la stringa dal buffer temporaneo a quello definitivo
    strncpy(string,temp,strlen(temp));

    return 0;
}

int intrequire(int16_t *num, int max, char *oggetto) {
    int counter = 0;
    int validate = max;
    if(validate == 0){
        counter = 1;
    } else {
        while (validate != 0) {
            validate /= 10;
            ++counter;
        }
    }
    char *buffer;
    validate = -1;
    char *endptr;
    while (validate != 0) {
        if ((buffer = (char *)malloc(sizeof(char) * (counter + 2))) == NULL) {
            printf("Errore: impossibile allocare spazio per buffer di input\n");
            return 1;
        }
        printf("Inserisci %s (massimo %d): \t", oggetto, max);
        if (fgets(buffer, counter + 2, stdin) == NULL) {
            printf("Errore: impossibile leggere da stdin\n");
            free(buffer);
            return 1;
        }
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        } else {
            fflush(stdin);
        }
        if (strlen(buffer) < 1) {
            printf("Non hai scritto nulla\n");
            validate = 1;
        } else {
            if ((strlen(buffer) == (size_t)(counter + 1)) && buffer[strlen(buffer) - 1] != '\n') {
                printf("Hai scritto troppi caratteri\n");
                validate = 1;
            } else {
                if ((strlen(buffer) == (size_t)(counter + 1)) && buffer[strlen(buffer) - 1] == '\n') {
                    buffer[strlen(buffer) - 1] = '\0';
                }
                *num = strtol(buffer, &endptr, 10);
                if (*endptr != '\0' && *endptr != '\n') {
                    printf("Input non valido. Inserisci un numero intero.\n");
                    validate = 1;
                } else {
                    if (*num < 0 || *num > max) {
                        printf("Il numero deve essere compreso tra 0 e %d.\n", max);
                        validate = 1;
                    } else {
                        validate = 0;
                    }
                }
            }
        }
        free(buffer);
    }
    return 0;
}
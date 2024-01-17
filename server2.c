#include "server2.h"

// voglio creare una cosa più complessa di mutex per bloccare la scrittura e la lettura sui file
/* posso fare semplicemente così, un mutex e un semaforo, un lettore controlla il mutex e se è disponibile allora rilascia un semaforo e lo riprende quando ha finito, lo scrittore invece blocca ilmutex e aspetta che non ci siano più semafori disponibili
        if (sem_trywait(&resource->rwSemaphore) == 0) {
            // Tentativo riuscito, entra nella sezione critica di lettura
            return 0;
        } else {
            // Tentativo non riuscito, aspetta un po' prima di ritentare
            usleep(100000);  // Aspetta 100 millisecondi (ad esempio)
            attempts++;
        }
        ma al contrario
*/
struct semFile chatsem;
struct semFile usersem;

Node* chatTable[MAX_TABLE];
Node* userTable[MAX_TABLE];

// questa funzione ha lo scopo di aggiornare tutti i file mantenedno solo le tuple valide e di creare le tabelle vuote per utenti e chat
int initDataBase(){
    //inizializziamo il file delle credenziali
    if(initFile(FILECRED) != 0){
        fprintf(stderr,"Errore impossibile inizializzare il file credenziali\n");
        return 1;
    }
    if(pthread_mutex_init(&(usersem.main),NULL) != 0){
        perror("Errore nell'inizializzazione del mutex per il file credentials");
        return 1;
    }
    if(sem_init(&chatsem.readers,0,0) != 0){
        perror("Errore nell'inizializzazione del semaforo per i readers in file chat");
        return 1;
    }
    //inizializziamo il file chat
    if(initFile(FILECHAT) != 0){
        fprintf(stderr,"Errore impossibile inizializzare il file chat\n");
        return 1;
    }
    if(pthread_mutex_init(&(chatsem.main),NULL) != 0){
        perror("Errore nell'inizializzazione del mutex per il file chat");
        return 1;
    }
    if(sem_init(&usersem.readers,0,0) != 0){
        perror("Errore nell'inizializzazione del semaforo per i readers in file credential");
        return 1;
    }
    //inizializziamo la tabella hash per le chat
    for(int i=0;i<MAX_CHATTABLE;i++){
        chatTable[i] = NULL;
        if(pthread_mutex_init(&(chatTable[i]->modify),NULL) != 0){
            perror("Errore nell'inizializzazione del mutex nella tabella chatTable");
            return 1;
        }
    }
    //inizializziamo la tabella hash per le credenziali utenti
    for(int j=0;j<MAX_USERTABLE;j++){
        userTable[i]=NULL;
        if(pthread_mutex_init(&(userTable[i]->modify),NULL) != 0){
            perror("Errore nell'inizializzazione del mutex nella tabella userTable");
            return 1;
        }
    }
    return 0;
}

// la funzione accetta il nome del file senza txt e lo inizializza se c'è un errore nel "travasare" i dati dal temporaneo all'originale ritorna 2
int initFile(char* nomeFile){
    //creo la stringa vera per aprire il file
    char pathfile[strlen(nomeFile)+5];
    strcpy(pathfile,nomeFile);
    strcat(pathfile,".txt");
    //apertura file originale
    FILE *originalfile = fopen(pathfile,"r+"); //apertura del file in RDWR all'inizio
    if(originalfile == NULL){
        //il file non esiste e lo crea
        originalfile = fopen(pathfile,"w");
        fclose(originalfile);
        return 0;
    }
    //crea il duplicato
    char pathtemp[strlen(pathfile)+strlen("temp")+1];
    strcpy(pathtemp,"temp");
    strcat(pathtemp,pathfile);
    FILE *dupfile = fopen(pathtemp,"w+"); //apertura in scrittura troncata 
    if(dupfile == NULL){
        fprintf(stderr,"Errore: non e' stato possibile aprire il file duplicato \n");
        fclose(originalfile);
        return 1;
    }
    int temp;
    while((temp = fgetc(originalfile)) != EOF){
        fputc(temp,dupfile);
    }
    // spostiamo il seek del dupfile all'inizio del file
    fseek(dupfile, 0, SEEK_SET);
    //chiudiamo il file orginale per riaprlirlo troncato
    fclose(originalfile);
    originalfile = fopen(pathfile,"w+");
    if(originalfile == NULL){
        fprintf(stderr,"Errore: non e' stato possibile aprire il file originale \n");
        fclose(dupfile);
        return 1;
    }
    // riscrive nel file originale solo le credenziali valide
    // leggo il primo carattere di ogni riga
    while ((temp = fgetc(dupfile)) != EOF) {
        // Verifica il primo carattere
        if (temp == '1') { // Se il primo carattere è '1'
            // copia la riga nel file originale
            fputc(temp,originalfile); // scriviamo il bit validate
            while(((temp = fgetc(dupfile)) != '\n') || (temp == EOF)){
                if(temp == EOF){
                    fprintf(stderr,"Errore durante la copia di una tupla valida\n");
                    fclose(originalfile);
                    fclose(dupfile);
                    return 1;
                }
                fputc(temp,originalfile);
            }
            fputc(temp,originalfile); //scriviamo il carattere nuova riga
        }
        // Altrimenti, se è '0', la riga viene saltata
        while(((temp = fgetc(dupfile)) != '\n') || (temp == EOF)){
            if(temp == EOF){
                fprintf(stderr,"Errore durante lo scarto di una tupla non valida\n");
                fclose(originalfile);
                fclose(dupfile);
                return 1;
            }
        }
    }
    // Chiude entrambi i file
    fclose(dupfile);
    fclose(originalfile);
    //per sicurezza viene eliminato il file temporaneo
    if(remove(pathtemp) != 0){
        perror("Errore, impossibile eliminare il file temporaneo");
        return 1;
    }
    return 0;
}

// Hash function
unsigned int hash_function(char* key) {
    unsigned int hash = 0;
    while (*key) {
        hash = (hash * 31) + (*key++);
    }
    return hash % HASH_SIZE;
}

// Funzione di confronto per Chat
int compareChat(const void* a, const char* key) {
    return strcmp(((Chat*)a)->chat_id, key);
}

// Funzione di confronto per Utente
int compareUtente(const void* a, const char* key) {
    return strcmp(((Utente*)a)->username, key);
}

//funzione per liberare una struttura chat
int rmChat(void* chat){
    //cast del puntatore
    Chat* chatptr = (Chat*)chat;
    //liberiamo la memoria allocata per il chat_id
    free(chat->chat_id);
    //distruggiamo il semaforo per scrivere un messaggio
    if(pthread_mutex_destroy(&(chat->sem.main)) != 0){
        perror("Errore nella distruzione del mutex per la scrittura di un file in fase di eliminazione");
        return 1;
    }
    if(sem_destroy(&(chat->sem.reader) != 0){
        perror("Errore nella distruzione del semaforo per i lettori del file");
        return 1;
    }
    //liberiamo la memoria riservata alla struttura dati chat
    free(chat);
    return 0;
}

//funzione per liberare la memoria di una struttura dati Utente
int rmUtente(void* user){
    //cast del puntatore
    Utente* userptr = (Utente*)user;
    //liberiamo la memoria per riservata per l'username
    free(user->username);
    free(user);
    return 0;
}

//funzione per eliminare un nodo da una tabella hash
int rmNode(Node** table, char* key, int (*compare)(const void*, const char*), int (*remove)(const void *)){
    //calcoliamo l'indice della riga
    unsigned int i = hash_function(key);
    //facciamo una ricerca del nodo da eliminare salvando anche il puntatore al precedente
    Node* prev = NULL;
    Node* current = table[i];
    while( current != NULL){
        //blocchiamo il mutex per la modifica
        if(pthread_mutex_lock(&(current->modify)) != 0){
            perror("Errore nello bloccare il mutex del nodo corrente");
            return 1;
        }
        //se current è il primo nodo passiamo al successivo
        if(current = table[i]){
            prev = current;
            current = prev->next;
        } else {
            if(compare(current->content,key) == 0){      //controlliamo se è il nodo giusto
                //current è il nodo che dobbiamo eliminare
                //eliminiamo il contenuto
                if(remove(current->content) != 0){
                    fprintf(stderr,"Errore durante l'eliminazione del contenuto del nodo\n");
                    return 1;
                }
                //spostiamo i puntatori
                prev->next = current->next;
                //facciamo la free del nodo corrente da eliminare
                free(current);
                //rilasciamo il mutex del precedente
                if(pthread_mutex_unlock(&(prev->modify)) != 0){
                    perror("Errore nel rilascio del mutex dopo eliminazione nodo successivo");
                    return 1;
                }
                return 0;
            }
            //liberiamo il mutex del nodo precedente
            if(pthread_mutex_unlock(&(prev->modify)) != 0){
                    perror("Errore nel rilascio del mutex nello scorrimento della lista");
                    return 1;
            }
            //ci spostiamo di un nodo avanti
            prev = current;
            current = prev->next;
        }
    }
    if(pthread_mutex_unlock(&(prev->modify)) != 0){
        perror("Errore nel rilascio del mutex dopo aver letto tutta la lista");
        return 1;
    }
    fprintf(stderr,"Errore il nodo da eliminare non e' stato trovato\n");
    return 2;
}

// funzione che cerca un nodo all'interno della sua hashtable
Node* searchNode(Node** table, char* key, int (*compare)(const void*, const char*)){
    //troviamo indice della riga
    unsigned int i = hash_function(key);
    //scorriamo la lista per trovare il nodo richiesto
    Node* current = table[i]->next;
    while(current != NULL){
        if(compare(current->content,key) == 0){
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// funzione che crea un nuovo nodo per una determinata struttura dati e lo aggiunge un testa alla lista
int addNode(Node** table, char* key, void* data, int (*compare)(const void*, const char*)){
    // troviamo indice per la riga
    unsigned int i = hash_function(key);
    //creiamo un nuovo nodo
    Node* newNode = malloc(sizeof(Node));
    if(newNode == NULL){
        fprintf(stderr,"Errore nella creazione di un nuovo nodo\n");
        return 1;
    }
    newNode->content = data;
    if(pthread_mutex_init(&(newNode->modify),NULL) != 0){
        perror("Errore inizializzazione mutex per il nuovo nodo creato");
        return 1;
    }
    // blocchiamo la scrittura sul primo nodo della lista
    if(pthread_mutex_lock(&(table[i]->modify)) != 0){
        perror("Errore nel blocco del mutex della testa della lista");
        return 1;
    }
    // modifichiamo i puntatori
    newNode->next = table[i]->next;
    table[i]->next = newNode;
    //rilasciamo il mutex per la testa della lista
    if(pthread_mutex_unlock(&(table[i]->modify)) != 0){
        perror("Errore nell'unlock mutex della testa della lista");
        return 1;
    }
    return 0;
}

//funzione che gestisce i mutex per la lettura su file
int startReadFile(struct semFile* sem){
    //prendiamoil mutex main
    if(pthread_mutex_lock(&(sem->main)) != 0){
        perror("Impossibile prendere il mutex main del file");
        return 1;
    }
    //inseriamo un semaforo all'interno del semaforo reader per indicare che è presente un lettore
    sem_post(&(sem->readers));
    //rilasciamo il mutex main
    if(pthread_mutex_unlock(&(sem->main)) != 0){
        perror("Impossibile rilascaire il mutex main del file");
        return 1;
    }
    return 0;
}

int endReadFile(struct semFile* sem){
    //riprendiamo il gettone del semaforo che avevamo rilasciato allo start
    sem_wait(&(sem->readers));
    return 0;
}

int startWriteFile(struct semFile* sem){
    //predniamo il mutex main
    if(pthread_mutex_lock(&(sem->main)) != 0){
        perror("Impossibile prendere il mutex main del file");
        return 1;
    }
    //aspetiamo che non ci siano più semafori reader disponibili
    while (sem_trywait(&sem->readers) == 0) {
        // Tentativo riuscito, abbimo preso un token quindi dobbiamo restituirlo
        sem_post(&(sem->readers));
    }
    // adesso siamo sicuri che non ci sono più lettori e non possono entrare poichè abbiamo il mutex main
    return 0;
}

int endWriteFile(struct semFile* sem){
    // rilasciamo il mutex main
    if(pthread_mutex_unlock(&(sem->main)) != 0){
        perror("Impossibile rilasciare il mutex main del file");
        return 1;
    }
    return 0;
}
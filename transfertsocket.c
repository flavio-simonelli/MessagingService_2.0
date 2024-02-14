#include "transfertsocket.h"

/* Questo file contiene tutte le funzioni che servono per trasferire dati attraverso una socket fra server e client */

/* Funzione per inviare una stringa in chiaro tramite socket */
int send_data(void *data, size_t size, int socket) {
    int sent_total = 0;
    int sent_partial;
    while(sent_total < (int)size){
        if ((sent_partial = send(socket, data + sent_total, size - sent_total, 0)) == -1) {
            perror("Errore nell'invio dei dati");
            return 1;
        }
        sent_total += sent_partial;
    }
    return 0;
}

/* Funzione per ricevere una stringa in chiaro tramtite socket */
int receive_data(void *data, size_t size, int socket) {
    int recv_total = 0;
    int recv_partial;
    while(recv_total<(int)size){
        if ((recv_partial = recv(socket, data + recv_total, size - recv_total, 0)) == -1) {
        perror("Errore nella ricezione dei dati");
        return 1;
        }
        recv_total += recv_partial;
    }
    return 0;
}

/* Funzione per inviare un intero in chiaro tramite socket */
int send_int(int value, int socket) {

    // Calcoliamo il numero di caratteri necessari per scrivere in una stringa il numero
    int num_chars = snprintf(NULL, 0, "%d", value); // se il primo parametro è specificato a null la funzione ha lo scopo di contare il numero di caratteri che sarebbero stati scritti    
    // Allochiamo un buffer per contenere la stringa
    char buffer[num_chars + 1];  // +1 per il terminatore di stringa nullo
    // Convertiamo il numero nella stringa
    snprintf(buffer, sizeof(buffer), "%d", value);
    // Inviamo il dato
    if (send_data(&buffer, num_chars+1, socket) != 0) {
        perror("Errore nell'invio dell'intero");
        return 1;
    }

    return 0;
}

// ricezione di un intero in formato network e trasformazione in hardware
int receive_int(int* num, int socket, size_t buffer_size) {
    char buffer[buffer_size];
    // Ricevi la stringa dalla socket
    if (receive_data(buffer, buffer_size, socket) != 0) {
        fprintf(stderr,"Errore nella ricezione della stringa(intero)\n");
        return 1;
    }
    // Converti la stringa in un intero
    *num = atoi(buffer);
    return 0;
}

/* Funzione per inviare una stringa criptata tramite socket */
int send_encrypted_data(int socket, const unsigned char *data, size_t data_length, const unsigned char *tx_key) {
    // Allocazione del buffer per i dati cifrati
    unsigned char encrypted_data[data_length + crypto_secretbox_MACBYTES];
    // Generazione di un nonce univoco (NONCE = Number Used Once)
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes(nonce, sizeof(nonce));
    // Cifratura dei dati
    if (crypto_secretbox_easy(encrypted_data, data, data_length, nonce, tx_key) != 0) {
        perror("Errore nella criptazione del dato");
        return 1;
    }
    if(send_data(nonce,sizeof(nonce),socket) != 0){
        fprintf(stderr,"Errore nell'invio del nonce \n");
        return 1;
    }
    if(send_data(encrypted_data,sizeof(encrypted_data),socket) != 0){
        fprintf(stderr,"Errore nell'invio dei dati criptati \n");
        return 1;
    }
    return 0;
}

/* Funzione per la ricezione e decriptazione di un dato */
int receive_encrypted_data(int socket, unsigned char *received_data, size_t max_data_length, const unsigned char *rx_key) {
    // Ricezione del nonce
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    if(receive_data(nonce,sizeof(nonce),socket) != 0){
        fprintf(stderr,"Errore nella ricezione del nonce \n");
        return 1;
    }
    // Ricezione dei dati cifrati
    unsigned char encrypted_data[max_data_length + crypto_secretbox_MACBYTES];
    if(receive_data(encrypted_data,sizeof(encrypted_data),socket)!= 0){
        fprintf(stderr,"Errore nella ricezione dei dati criptati \n");
        return 1;
    }
    // Decifratura dei dati
    if (crypto_secretbox_open_easy(received_data, encrypted_data, sizeof(encrypted_data), nonce, rx_key) != 0) {
        perror("Decryption failed");
        return 1;
    }
    return 0;
}

int receive_encrypted_int(int socket, int* num, size_t sizenum, const unsigned char *rx_key) {
    // Ricevi l'intero cifrato
    unsigned char buffer[sizenum+1]; //numero di caratteri necessario più il terminatore della stringa
    if(receive_encrypted_data(socket, buffer, sizeof(buffer), rx_key) != 0){
        fprintf(stderr,"Errore nella ricezione dell'intero criptato \n");
        return 1;
    }
    // Converti la stringa in un intero
    *num = atoi((char*)buffer);
    return 0;
}

int send_encrypted_int(int socket, int value, size_t max_len, const unsigned char *tx_key) {
    //conversione dell'intero in una stringa
    char buffer[max_len+1];
    int size = snprintf(NULL,0,"%d",value);
    snprintf(buffer,max_len+1,"%0*d",(int)(max_len-size),value); // questa funzione converte il numero in una stringa aggiungendo 0 davanti ad esso per avere una stringa di dimensione fissa
    // Invia l'intero cifrato
    if(send_encrypted_data(socket, (const unsigned char *)buffer, sizeof(buffer), tx_key) != 0){
        fprintf(stderr, "Errore nell'invio dell'intero criptato \n");
        return 1;
    }
    return 0;
}
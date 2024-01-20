#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <sodium.h>
#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif

int send_data(void *data, size_t size, int socket);
int receive_data(void *data, size_t size, int socket);

int send_int(int value, int socket);
int receive_int(int* num, int socket, size_t buffer_size);

int send_encrypted_data(int socket, const unsigned char *data, size_t data_length, const unsigned char *tx_key);
int receive_encrypted_data(int socket, unsigned char *received_data, size_t max_data_length, const unsigned char *rx_key);

int receive_encrypted_int(int socket, int* num, size_t sizenum, const unsigned char *rx_key);
int send_encrypted_int(int socket, int value, size_t max_len, const unsigned char *tx_key);


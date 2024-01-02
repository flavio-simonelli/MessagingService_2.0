CC = gcc
CFLAGS = -Wall -Wextra -lpthread -lsodium

.PHONY: all
all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

.PHONY: clean
clean:
	rm -f server client

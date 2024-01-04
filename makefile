CC = gcc
CFLAGS = -Wall -Wextra -lpthread -lsodium

.PHONY: all
all: server client

server: server.c
	$(CC) server.c -o server $(CFLAGS)

client: client.c
	$(CC) client.c -o client $(CFLAGS)

.PHONY: clean
clean:
	rm -f server client

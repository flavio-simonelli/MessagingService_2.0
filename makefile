CC = gcc
CFLAGS = -Wall -Wextra -lpthread -lsodium

.PHONY: all
all: server client

server: server.c
	$(CC) server.c transfertsocket.c -o server $(CFLAGS)

client: client.c
	$(CC) client.c transfertsocket.c -o client $(CFLAGS)

.PHONY: clean
clean:
	rm -f server client

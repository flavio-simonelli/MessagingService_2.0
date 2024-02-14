CC = gcc
CFLAGS = -Wall -Wextra -lpthread -lsodium -g

.PHONY: all
all: server client

server: server2.c
	$(CC) server2.c transfertsocket.c -o server $(CFLAGS)

client: client.c
	$(CC) client.c transfertsocket.c inputuser.c -o client $(CFLAGS)

.PHONY: clean
clean:
	rm -f server client

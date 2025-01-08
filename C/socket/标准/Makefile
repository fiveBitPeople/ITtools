CC = gcc
CFLAGS = -Wall -pthread -Iinclude -I./ -std=gnu99
LDFLAGS = -lrt

all: server client

server: src/server.c src/functions.c src/log.c src/network.c
	$(CC) $(CFLAGS) -o server src/server.c src/functions.c src/log.c src/network.c $(LDFLAGS)

client: src/client.c src/log.c src/network.c
	$(CC) $(CFLAGS) -o client src/client.c src/log.c src/network.c $(LDFLAGS)

clean:
	rm -f server client

CC = gcc
CFLAGS = -Wall -pthread

all: server dns_server

server: main.c protocol.c game.c
	$(CC) $(CFLAGS) -o server main.c protocol.c game.c

dns_server: dns_server.c
	$(CC) $(CFLAGS) -o dns_server dns_server.c

clean:
	rm -f server dns_server
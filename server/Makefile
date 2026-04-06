CC = gcc
CFLAGS = -Wall -pthread

all: server

server: main.c protocol.c game.c
	$(CC) $(CFLAGS) -o server main.c protocol.c game.c

clean:
	rm -f server
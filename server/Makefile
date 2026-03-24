CC = gcc
CFLAGS = -Wall -pthread

all: server

server: main.c
	$(CC) $(CFLAGS) -o server main.c

clean:
	rm -f server
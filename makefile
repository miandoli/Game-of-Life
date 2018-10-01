CC=gcc
CFLAGS= -g -Wall -lpthread

life: life.c
	$(CC) -o life life.c $(CFLAGS)

clean:
	rm -f life

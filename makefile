CC=gcc
CFLAGS=-Wall -Wextra -std=c99 -pedantic
LIBS=-lm -lX11

all: eng

eng: main.o eng.o
	$(CC) -o eng main.o eng.o $(LIBS)

main.o: main.c eng.h
	$(CC) $(CFLAGS) -c main.c

eng.o: eng.c eng.h
	$(CC) $(CFLAGS) -c eng.c

clean:
	rm -f eng *.o

.PHONY: all clean

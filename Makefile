CC = gcc
CFLAGS = -c -g


all: cshell clean

cshell: cshell.o
	$(CC) cshell.o -o cshell

cshell.o: src/cshell.c
	$(CC) $(CFLAGS) src/cshell.c

clean:
	rm *.o
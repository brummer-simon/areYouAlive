CC=gcc
CFLAGS=-g -Wall

all: areYouAlive


areYouAlive: areYouAlive.c
	$(CC) $(CFLAGS) areYouAlive.c -o areYouAlive	    

clean:
	rm -rf *o areYouAlive

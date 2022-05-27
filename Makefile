CC = gcc
CFLAGS = -Wall -pedantic -g -O3


all: forkit tryit

forkit: forkit.o
	$(CC) $(CFLAGS) -o forkit forkit.o
forkit.o: forkit.c
	$(CC) $(CFLAGS) -c -o forkit.o forkit.c

tryit: tryit.o
	$(CC) $(CFLAGS) -o tryit tryit.o
tryit.o: tryit.c
	$(CC) $(CFLAGS) -c -o tryit.o tryit.c


clean:
	rm -f *.o

server:
	ssh acheun29@unix1.csc.calpoly.edu

upload:
	scp -r ../csc357-asgn6 acheun29@unix1.csc.calpoly.edu:csc357

download:
	scp -r acheun29@unix1.csc.calpoly.edu:csc357/csc357-asgn6 /Users/andrewcheung/Documents/Cal\ Poly\ Stuff/csc357


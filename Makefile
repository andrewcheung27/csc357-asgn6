CC = gcc
CFLAGS = -Wall -pedantic -g -O3


all: mush


mush: mush.o
	$(CC) $(CFLAGS) -o mush -L ~pn-cs357/Given/Mush/lib64 mush.o -lmush
mush.o: mush.c
	$(CC) $(CFLAGS) -c -o mush.o -I ~pn-cs357/Given/Mush/include mush.c


clean:
	rm -f *.o

server:
	ssh acheun29@unix1.csc.calpoly.edu

upload:
	scp -r ../csc357-asgn6 acheun29@unix1.csc.calpoly.edu:csc357

download:
	scp -r acheun29@unix1.csc.calpoly.edu:csc357/csc357-asgn6 /Users/andrewcheung/Documents/Cal\ Poly\ Stuff/csc357


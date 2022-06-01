CC = gcc
CFLAGS = -Wall -pedantic -g -O3


all: mush2


mush2: mush2.o
	$(CC) $(CFLAGS) -o mush2 -L ~pn-cs357/Given/Mush/lib64 mush2.o -lmush
mush2.o: mush2.c
	$(CC) $(CFLAGS) -c -o mush2.o -I ~pn-cs357/Given/Mush/include mush2.c


clean:
	rm -f *.o

server:
	ssh acheun29@unix1.csc.calpoly.edu

upload:
	scp README Makefile mush2.c acheun29@unix1.csc.calpoly.edu:csc357/csc357-asgn6

download:
	scp acheun29@unix1.csc.calpoly.edu:csc357/csc357-asgn6/"README Makefile mush2.c" .


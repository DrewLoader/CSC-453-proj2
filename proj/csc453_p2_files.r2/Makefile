CC = gcc
CFLAGS = -Wall -g -I.
AR = ar

liblwp.a: lwp.o magic64.o AlwaysZero.o
	$(AR) rcs liblwp.a lwp.o magic64.o AlwaysZero.o

lwp.o: lwp.c lwp.h fp.h
	$(CC) $(CFLAGS) -c lwp.c

magic64.o: magic64.S
	$(CC) $(CFLAGS) -c magic64.S

AlwaysZero.o: AlwaysZero.c lwp.h schedulers.h
	$(CC) $(CFLAGS) -c AlwaysZero.c

numbersmain: numbersmain.c liblwp.a
	$(CC) $(CFLAGS) -o numbersmain numbersmain.c -L. -llwp

randomsnakes: randomsnakes.c liblwp.a HackySnakeSchedulers.o
	$(CC) $(CFLAGS) -o randomsnakes randomsnakes.c HackySnakeSchedulers.o -L. -llwp -L. -lsnakes -lncurses

hungrysnakes: hungrysnakes.c liblwp.a HackySnakeSchedulers.o
	$(CC) $(CFLAGS) -o hungrysnakes hungrysnakes.c HackySnakeSchedulers.o -L. -llwp -L. -lsnakes -lncurses

clean:
	rm -f lwp.o magic64.o AlwaysZero.o liblwp.a numbersmain randomsnakes hungrysnakes

.PHONY: clean
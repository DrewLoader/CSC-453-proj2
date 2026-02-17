CC = gcc
CFLAGS = -Wall -g -I.
AR = ar

liblwp.a: lwp.o magic64.o
	$(AR) rcs liblwp.a lwp.o magic64.o

lwp.o: lwp.c lwp.h fp.h
	$(CC) $(CFLAGS) -c lwp.c

magic64.o: magic64.S
	$(CC) $(CFLAGS) -c magic64.S

clean:
	rm -f lwp.o magic64.o liblwp.a

.PHONY: clean
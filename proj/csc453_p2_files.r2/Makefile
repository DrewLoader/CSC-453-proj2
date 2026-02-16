CC = gcc
CFLAGS = -Wall -g
AR = ar

liblwp.a: lwp.o magic64.o
	$(AR) rcs liblwp.a lwp.o magic64.o

lwp.o: lwp.c lwp.h fp.h
	$(CC) $(CFLAGS) -c lwp.c

magic64.o: magic64.S
	$(CC) $(CFLAGS) -c magic64.S

clean:
	rm -f *.o liblwp.a
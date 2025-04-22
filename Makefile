# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC= gcc
CFLAGS= -g -Wall -std=gnu99
LIBS = 

OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o pdu.o

all:   cClient server serverPoll

cClient: cClient.c $(OBJS)
	$(CC) $(CFLAGS) -o cClient cClient.c  $(OBJS) $(LIBS)

server: server.c $(OBJS)
	$(CC) $(CFLAGS) -o server server.c $(OBJS) $(LIBS)

serverPoll: serverPoll.c $(OBJS)
	$(CC) $(CFLAGS) -o serverPoll serverPoll.c $(OBJS) $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

pdu: pdu.c
	$(CC) $(CFLAGS) -c pdu.c

cleano:
	rm -f *.o

clean:
	rm -f server cClient serverPoll *.o





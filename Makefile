# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC= gcc
CFLAGS= -g -Wall -std=gnu99
LIBS = 

OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o pdu.o safePDU.o printBytes.o handleTable.o getPDUdata.o commandParse.o

all:   cclient server

# CHANGED CODE: Modified the name of cclient
cclient: cclient.c $(OBJS)
	$(CC) $(CFLAGS) -o cclient cclient.c  $(OBJS) $(LIBS)

server: server.c $(OBJS)
	$(CC) $(CFLAGS) -o server server.c $(OBJS) $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

pdu: pdu.c
	$(CC) $(CFLAGS) -c pdu.c

handleTableTests: handleTable
	$(CC) $(CFLAGS) -o handleTableTests handleTableTests.c handleTable.o

getPDUdataTests: getPDUdata.o getPDUdataTests.c
	$(CC) $(CFLAGS) -o getPDUdataTests getPDUdataTests.c getPDUdata.o

commandParseTests: commandParse.o commandParseTests.c printBytes.o
	$(CC) $(CFLAGS) -o commandParseTests commandParseTests.c commandParse.o printBytes.o

cleano:
	rm -f *.o

clean:
	rm -f server cclient serverPoll *.o





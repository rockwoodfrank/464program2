/******************************************************************************
* myClient.c
*
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include "networks.h"
#include "safeUtil.h"
#include "pdu.h"
#include "pollLib.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1

void sendToServer(int socketNum);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void clientControl(char* serverName, char* serverPort);
void processStdIn(int socketNum);
void processMsgFromServer(int socketNum);

int main(int argc, char * argv[])
{	
	checkArgs(argc, argv);

	clientControl(argv[1], argv[2]);

	// close(socketNum);
	
	return 0;
}

void clientControl(char* serverName, char* serverPort)
{
	int socketNum = 0;
	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(serverName, serverPort, DEBUG_FLAG);
	
	// Starting the polling
	setupPollSet();
	addToPollSet(socketNum);
	// Adding the stdin
	addToPollSet(STDIN_FILENO);

	while(1)
	{
		printf("Enter data: ");
		fflush(stdout);
		int readyFD = pollCall(-1);
		if (readyFD == STDIN_FILENO)
			processStdIn(socketNum);
		else if (readyFD == socketNum)
			processMsgFromServer(socketNum);
	}

}

void processStdIn(int socketNum)
{
	uint8_t buffer[MAXBUF];   //data buffer
	int sendLen = 0;        //amount of data to send
	int sent = 0;            //actual amount of data sent/* get the data and send it   */


	sendLen = readFromStdin(buffer);
	printf("read: %s string len: %d (including null)\n", buffer, sendLen);

	sent =  sendPDU(socketNum, buffer, sendLen);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	printf("Socket:%d: Sent, Length: %d msg: %s\n", socketNum, sent, buffer);
}

void processMsgFromServer(int socketNum)
{
	int recvBytes = 0;
	uint8_t buffer[MAXBUF];   //data buffer

	// just for debugging, recv a message from the server to prove it works.
	recvBytes = recvPDU(socketNum, buffer, MAXBUF);
	if (recvBytes < 0)
	{
		perror("recv call");
		exit(-1);
	}
	if (recvBytes > 0)
	{
		printf("Socket %d: Byte recv: %d message: %s\n", socketNum, recvBytes, buffer);
	}
	else
	{
		printf("Server terminated\n");
		exit(-1);
	}
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';

	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 3)
	{
		printf("usage: %s host-name port-number \n", argv[0]);
		exit(1);
	}
}

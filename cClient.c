/******************************************************************************
* cClient.c
*
* Adapted from code by Prof. Smith
* Written by Rockwood Frank - Apr 2025
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
#include <ctype.h>

#include "networks.h"
#include "safeUtil.h"
#include "safePDU.h"
#include "pdu.h"
#include "pollLib.h"
#include "pduFlags.h"
#include "printBytes.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1
#define MAX_HANDLE_NAME 100
#define MAX_MESSAGE_SIZE 200


void sendToServer(int socketNum);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void clientControl(char* clientHandle, char* serverName, char* serverPort);
void processStdIn(int socketNum, char *c_handle);
int genPackets(int socketNum, char *c_handle, uint8_t *input_buffer, int input_len, uint8_t msgFlag);
int parseCommand(uint8_t* userInput, int inputLen);
void processMsgFromServer(int socketNum);
int sendConnect(int socketNum, char *handle, int h_len);
void initConnection(char *handleName, int handleLen, int socketNum);
int getConnectResp(int socketNum);
int formatPacket(uint8_t *packet, uint8_t flag, uint8_t h_len, uint8_t *header);
int appendHandle(uint8_t * stream, uint8_t *handle);
int getNextHandle(uint8_t *stream, uint8_t *dest);
int addDestHandles(uint8_t msgFlag, uint8_t *in_buf, int *in_buf_pos, uint8_t *out_buf);
int sendCast(int socketNum, uint8_t *cast_header, int cast_len, uint8_t *payload_buffer, int payload_len);


int main(int argc, char * argv[])
{	
	checkArgs(argc, argv);

	clientControl(argv[1], argv[2], argv[3]);

	// close(socketNum);
	
	return 0;
}

void clientControl(char* clientHandle, char* serverName, char* serverPort)
{
	int socketNum = 0;
	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(serverName, serverPort, DEBUG_FLAG);
	int handleLen = strlen(clientHandle);

	// Initialize the connection
	initConnection(clientHandle, handleLen, socketNum);
	printf("Connection created.\n");
	
	// Starting the polling
	setupPollSet();
	addToPollSet(socketNum);
	addToPollSet(STDIN_FILENO);

	printf("$: ");
	fflush(stdout);
	while(1)
	{
		int readyFD = pollCall(-1);
		if (readyFD == STDIN_FILENO)
		{
			processStdIn(socketNum, clientHandle);
			printf("Enter data: ");
			fflush(stdout);
		}
		else if (readyFD == socketNum)
			processMsgFromServer(socketNum);
	}

}

void processStdIn(int socketNum, char *c_handle)
{
	uint8_t stdin_buffer[MAXBUF];   //data buffer
	int stdin_len = 0;        		//amount of data to send
	// int sent = 0;            		//actual amount of data sent/* get the data and send it   */
	uint8_t msgFlag;


	// TODO: Handling messages larger than 1400 characters
	stdin_len = readFromStdin(stdin_buffer);
	msgFlag = parseCommand(stdin_buffer, stdin_len);
	if (msgFlag != 0)
	{
		genPackets(socketNum, c_handle, &(stdin_buffer[3]), stdin_len-3, msgFlag);
	}
}

int genPackets(int socketNum, char *c_handle, uint8_t *input_buffer, int input_len, uint8_t msgFlag)
{
	uint8_t packet_buffer[MAXBUF];	// The buffer of information to be put to sendPDU
	int packet_len = 0;				// The position of the end of the buffer
	int input_pos = 0;
	uint8_t nullChar = '\0';
	int sent = 0;

	// Append the message flag 
	memcpy(packet_buffer, &msgFlag, sizeof(uint8_t));
	packet_len += 1;
	// Append the client handle
	int h_len = appendHandle(&(packet_buffer[1]), c_handle);
	packet_len += h_len;


	if (msgFlag == PDU_HANDLES)
	{
		// Null terminate the string and send it
		memcpy(&(packet_buffer[packet_len]), &nullChar, sizeof(uint8_t));
		packet_len++;
		sent =  safeSendPDU(socketNum, packet_buffer, packet_len );
	}
	else
	{
		// If it's a unicast or a multicast
		if (msgFlag == PDU_UNICAST || msgFlag == PDU_MULTICAST)
		{
			packet_len += addDestHandles(msgFlag, input_buffer, &input_pos, &(packet_buffer[packet_len]));
		}
		printBytes(packet_buffer, 25);
		printf("Packet length is %d\n", packet_len);
		// Get the message size and send multiple if needed
		int remainingData = input_len - input_pos;
		// Becuase the greatest message size if 200 bytes, break it up if needed
		// TODO: Handling a message that's exactly 200 bytes long(ends w/ a null character)
		int packetsToSend = (remainingData / 200) + 1;
		for (int i = 0; i < packetsToSend; i++)
		{
			// TODO: Break thsi up into 200-sized chunks
			sent += sendCast(socketNum, packet_buffer, packet_len, &(input_buffer[input_pos]), remainingData);
		}
	}
	return sent;
}

int sendCast(int socketNum, uint8_t *cast_header, int cast_len, uint8_t *payload_buffer, int payload_len)
{
		// Copy over the heading info
		uint8_t sending_buffer[cast_len + 200];
		memcpy(sending_buffer, cast_header, sizeof(uint8_t) * cast_len);
		// Append the message
		memcpy(&(sending_buffer[cast_len]), payload_buffer, sizeof(uint8_t) * payload_len);
		// stdin_pos += dataToSend;
		// sending_pos += dataToSend;
		// // Null terminate the string - TODO: if necessary?
		// memcpy(&(sending_buffer[sending_pos]), &nullChar, sizeof(uint8_t));
		// Send it off
		int sent = safeSendPDU(socketNum, sending_buffer, cast_len+payload_len);
		return sent;
}

/* 	Add destination handles to the buffer, starting at the top.
	Assumes that it can write anywhere in the buffer.
	Returns the amount of bytes added. */
int addDestHandles(uint8_t msgFlag, uint8_t *in_buf, int *in_buf_pos, uint8_t *out_buf)
{
	uint8_t num_handles = 0;
	int out_buf_len = 0;
	// If it's a multicast, get the number of handles from the string
	if (msgFlag == PDU_MULTICAST)
	{
		// Converts the char to a number
		// TODO: Verifying the input is a number so that the program doesn't segfault
		num_handles = in_buf[0] - '0';
		*in_buf_pos += 2;
	}
	// Otherwise, the number of handles is one.
	else num_handles = 1;
	// Append this to the string
	memcpy(out_buf, &num_handles, sizeof(uint8_t));
	out_buf_len ++;
	// Append each handle
	for (int i = 0; i < num_handles; i++)
	{
		uint8_t nextHandle[MAX_HANDLE_NAME];
		uint8_t nextHandleLen = getNextHandle(&(in_buf[*in_buf_pos]), nextHandle);
		// Go up by the handle's length and then one for the space
		*in_buf_pos += nextHandleLen + 1;
		out_buf_len += appendHandle(&(out_buf[out_buf_len]), nextHandle);
	}
	return out_buf_len;
}

void processMsgFromServer(int socketNum)
{
	int recvBytes = 0;
	uint8_t buffer[MAXBUF];   //data buffer

	// just for debugging, recv a message from the server to prove it works.
	recvBytes = safeRecvPDU(socketNum, buffer, MAXBUF);
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
	if (argc != 4)
	{
		printf("usage: %s handle server-name server-port \n", argv[0]);
		exit(1);
	}
	// TODO: verifying the handle name is less than the max length
}


int sendConnect(int socketNum, char *handle, int h_len)
{
	uint8_t packet[h_len + 2];
	// Placing the flag
	uint8_t flag = PDU_CONNECT;
	memcpy(packet, &flag, sizeof(uint8_t));

	// Placing the handle length
	uint8_t len_b = h_len;
	memcpy(&(packet[1]), &len_b, sizeof(uint8_t));

	// Placing the handle
	memcpy(&(packet[2]), handle, len_b);

	int sent = safeSendPDU(socketNum, packet, h_len + 2);
	return sent;
}

void initConnection(char *handleName, int handleLen, int socketNum)
{
	// Send a connect
	sendConnect(socketNum, handleName, handleLen);

	// Block until a 2 or 3 is received
	int response = 0;
	while (response != PDU_ACCEPT) response = getConnectResp(socketNum);
}

// Getting the connection response from the server
int getConnectResp(int socketNum)
{
	int recvBytes = 0;
	uint8_t buffer[MAXBUF];   //data buffer

	recvBytes = safeRecvPDU(socketNum, buffer, MAXBUF);
	uint8_t serverResp = buffer[0];

	// 3? stop program
	if (serverResp == PDU_REJECT)
	{
		perror("Server rejected client");
		exit(3);
	}
	// 2? allow connection
	else return serverResp;
}

// Parse the user's input and figure out what to do next
int parseCommand(uint8_t* userInput, int inputLen)
{
	// Verify the input is at least 3(%x + null) characters
	// and verify the input begins with a percent sign
	if (inputLen < 3 || userInput[0] != '%')
	{
		printf("Enter a commanded preceded by %%.\n");
		return 0;
	}
	// %M - Messsages
	if (userInput[1] == 'M' || userInput[1] == 'm')
		return PDU_UNICAST;
	// %B - Broadcast
	else if (userInput[1] == 'B' || userInput[1] == 'b')
		return PDU_BROADCAST;
	// %C - Multicast
	else if (userInput[1] == 'C' || userInput[1] == 'c')
		return PDU_MULTICAST;
	// %L - List
	else if (userInput[1] == 'L' || userInput[1] == 'l')
		return PDU_HANDLES;
	else 
	{
		printf("Enter %%M, %%B, %%C, or %%L.\n");
		return 0;
	}
}

// Add the flag number, length of handle, and client handle. Returns
// the length of the added data
// int formatPacket(uint8_t *packet, uint8_t flag, uint8_t h_len, uint8_t *header)
// {
// 	// The flag
// 	memcpy(packet, &flag, sizeof(uint8_t));
// 	// The handle length
// 	memcpy(&(packet[1]), &h_len, sizeof(uint8_t));
// 	// The Handle
// 	memcpy(&(packet[2]), header, sizeof(uint8_t) * h_len);

// 	return 2 + h_len;
// }

// Append the handle(with its length) to the stream at pos.
int appendHandle(uint8_t * stream, uint8_t *handle)
{
	// Have to subtract one to remove the null character
	uint8_t handleLen = strlen(handle);
	memcpy(stream, &handleLen, sizeof(uint8_t));
	memcpy(&(stream[1]), handle, sizeof(uint8_t) * handleLen);
	return handleLen + 1;
}

// Get the next handle from a stream of bytes by finding the next space or null character.
// Returns the handle length.
int getNextHandle(uint8_t *stream, uint8_t *dest)
{
	int i = 0;
	
	while(!isspace(stream[i]) && stream[i] != 0) 
	{
		i++;
	}
	memcpy(dest, stream, sizeof(uint8_t) * i);
	// Null Terminate
	uint8_t nullTerm = '\0';
	memcpy(&(dest[i+1]), &nullTerm, sizeof(uint8_t));
	return i;
}

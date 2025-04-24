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
#include "getPDUdata.h"
#include "commandParse.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1
#define MAX_HANDLE_NAME 100
#define MAX_MESSAGE_SIZE 200


void sendToServer(int socketNum);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void clientControl(char* clientHandle, char* serverName, char* serverPort);
void processStdIn(int socketNum, char *c_handle);
int genPackets(int socketNum, char *c_handle, Command *command);
// int parseCommand(uint8_t* userInput, int inputLen);
void processMsgFromServer(int socketNum);
int sendConnect(int socketNum, char *handle, int h_len);
void initConnection(char *handleName, int handleLen, int socketNum);
int getConnectResp(int socketNum);
int formatPacket(uint8_t *packet, uint8_t flag, uint8_t h_len, uint8_t *header);
int addDestHandles(Command *command, uint8_t *out_buf);
int sendCast(int socketNum, uint8_t *cast_header, int cast_len, Command *command, int payload_len);

// Receiving
void printChat(uint8_t *packet, int len);


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

	while(1)
	{
		printf("$: ");
		fflush(stdout);
		int readyFD = pollCall(-1);
		if (readyFD == STDIN_FILENO)
		{
			processStdIn(socketNum, clientHandle);

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

	// TODO: Handling messages larger than 1400 characters
	stdin_len = readFromStdin(stdin_buffer);
	Command currentCommand;
	commandParse(stdin_buffer, stdin_len, &currentCommand);
	if (currentCommand.flag != 0)
	{
		genPackets(socketNum, c_handle, &currentCommand);
	}
}

int genPackets(int socketNum, char *c_handle, Command *command)
{
	uint8_t packet_buffer[MAXBUF];	// The buffer of information to be put to sendPDU
	int packet_len = 0;				// The position of the end of the buffer
	uint8_t nullChar = '\0';
	int sent = 0;
	// Append the message flag 
	memcpy(get_flag(packet_buffer), &(command->flag), sizeof(uint8_t));
	packet_len += 1;
	// Append the client handle's length
	uint8_t h_len = strlen(c_handle);
	memcpy(get_src_len(packet_buffer), &h_len, sizeof(uint8_t));
	packet_len += 1;
	// Append the client's handle
	memcpy(get_src(packet_buffer), c_handle, sizeof(uint8_t) * h_len);
	packet_len += h_len;

	if (command->flag == PDU_HANDLES)
	{
		// Null terminate the string and send it
		memcpy(get_msg(packet_buffer), &nullChar, sizeof(uint8_t));
		packet_len++;
		sent =  safeSendPDU(socketNum, packet_buffer, packet_len);
	}
	else
	{
		// If it's a unicast or a multicast
		if (command->flag == PDU_UNICAST || command->flag == PDU_MULTICAST)
		{
			packet_len += addDestHandles(command, packet_buffer);
		}
		// Get the message size and send multiple if needed
		int msgLen = command->msgLen;
		// Becuase the greatest message size if 200 bytes, break it up if needed
		// TODO: Handling a message that's exactly 200 bytes long(ends w/ a null character)
		int packetsToSend = (msgLen / 200) + 1;
		for (int i = 0; i < packetsToSend; i++)
		{
			// TODO: Break thsi up into 200-sized chunks
			sent += sendCast(socketNum, packet_buffer, packet_len, command, msgLen);
		}
	}
	return sent;
}

int sendCast(int socketNum, uint8_t *cast_header, int cast_len, Command *command, int payload_len)
{
		// Copy over the heading info
		uint8_t sending_buffer[cast_len + 200];
		memcpy(sending_buffer, cast_header, sizeof(uint8_t) * cast_len);
		// Append the message
		memcpy(get_msg(sending_buffer), command->msg, sizeof(uint8_t) * payload_len);
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
int addDestHandles(Command *command, uint8_t *out_buf)
{
	uint8_t num_handles = command->destsLen;
	int out_buf_len = 0;
	// TODO: Verifying the input is a number so that the program doesn't segfault
	// Append the number of destinations to the output
	memcpy(get_num_dests(out_buf), &num_handles, sizeof(uint8_t));
	out_buf_len ++;
	// Append each handle
	for (int i = 0; i < num_handles; i++)
	{
		// Copy over the length of the handle
		memcpy(get_nth_dest_len(out_buf, i), &(command->lengths[i]), sizeof(uint8_t));
		// Copy over the handle
		memcpy(get_nth_dest(out_buf, i), command->dests[i], sizeof(uint8_t) * command->lengths[0]);
		// Appending the length of the new handle plus 1 for its length
		out_buf_len += *(get_nth_dest_len(out_buf, i)) + 1;
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
		// printf("Socket %d: Byte recv: %d message: %s\n", socketNum, recvBytes, buffer);
		handleMsg(buffer, recvBytes);
	}
	else
	{
		printf("Server terminated\n");
		exit(-1);
	}
}

void handleMsg(uint8_t *packet, int len)
{
	uint8_t flag = *(get_flag(packet));
	// Error message
	if (flag == PDU_ERROR)
	{
		//
	}
	// Chat message
	else if (flag == PDU_UNICAST || flag == PDU_MULTICAST || flag == PDU_BROADCAST)
	{
		printChat(packet, len);
	}
	// Handle message
	else if (flag == PDU_ERROR)
	{
		//
	}
}

void printChat(uint8_t *packet, int len)
{
	// print a newline
	// Get the sender name
	uint8_t sender[MAX_HANDLE_NAME];
	sender_name_toStr(packet, sender);
	printf("\n%s: %s\n", sender, get_msg(packet));
}

void sender_name_toStr(uint8_t *packet, uint8_t *dest)
{
	uint8_t sender_len = *(get_src_len(packet));
	memcpy(dest, get_src(packet), sizeof(uint8_t) * sender_len);
	dest[sender_len] = '\0';
}

void print_msg(uint8_t *packet, int len)
{
	uint8_t msg = get_msg(packet);
	printf(msg);
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
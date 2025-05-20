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

#define MAXBUF 1400
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
int getConnectResp(int socketNum, char* handleName);

// Sending
int formatPacket(uint8_t *packet, uint8_t flag, uint8_t h_len, uint8_t *header);
int addDestHandles(Command *command, uint8_t *out_buf);
int sendCast(int socketNum, uint8_t *cast_header, int cast_len, Command *command, int payload_len, int byte_offset);
int sendMessage(Command *command, uint8_t *packet_buffer, int packet_len);

// Receiving
void printChat(uint8_t *packet, int len);
void handleMsg(uint8_t *packet, int len);
int sender_name_toStr(uint8_t *packet, uint8_t *dest);
void printErrorMsg(uint8_t *packet, int len);

// Recieving Handles
void receive_handles();
void print_htable_len(uint8_t *packet);
uint8_t parse_h_msg(uint8_t *buffer, int buffer_len);
void print_handle(uint8_t *buffer);

int socketNum = 0;

int main(int argc, char * argv[])
{	
	checkArgs(argc, argv);

	clientControl(argv[1], argv[2], argv[3]);

	// close(socketNum);
	
	return 0;
}

void clientControl(char* clientHandle, char* serverName, char* serverPort)
{
	socketNum = 0;
	int handleLen = strlen(clientHandle);
	if (handleLen > 100)
	{
		printf("Invalid handle, handle longer than 100 characters: %s\n", clientHandle);
		exit(-1);
	}
	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(serverName, serverPort, DEBUG_FLAG);


	// Initialize the connection
	initConnection(clientHandle, handleLen, socketNum);
	
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
	stdin_len = readFromStdin(stdin_buffer);
	if (stdin_len != -1)
	{
		int p_result;
		Command currentCommand;
		p_result = commandParse(stdin_buffer, stdin_len, &currentCommand);
		if (currentCommand.flag != 0 && p_result == 0)
		{
			genPackets(socketNum, c_handle, &currentCommand);
		}
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
		// Enter handle receive mode
		receive_handles();
	}
	else
	{
		sendMessage(command, packet_buffer, packet_len);
	}
	return sent;
}

int sendMessage(Command *command, uint8_t *packet_buffer, int packet_len)
{
	int sent = 0;
	// If it's a unicast or a multicast
	if (command->flag == PDU_UNICAST || command->flag == PDU_MULTICAST)
	{
		packet_len += addDestHandles(command, packet_buffer);
	}
	
	// Get the message size and send multiple if needed
	int msgLen = command->msgLen;
	int bytesRemaining = msgLen;
	int bytePosition = 0;
	// Becuase the greatest message size is 200 bytes, break it up if needed
	while(bytesRemaining > 200)
	{
		sent += sendCast(socketNum, packet_buffer, packet_len, command, 200, bytePosition);
		bytePosition += 199;
		bytesRemaining -= 199;
	}
	sent += sendCast(socketNum, packet_buffer, packet_len, command, bytesRemaining, bytePosition);
	return sent;
}

int sendCast(int socketNum, uint8_t *cast_header, int cast_len, Command *command, int payload_len, int byteOffset)
{
		// Copy over the heading info
		uint8_t sending_buffer[cast_len + 200];
		int sent = 0;
		memcpy(sending_buffer, cast_header, sizeof(uint8_t) * cast_len);
		// Append the message
		if (payload_len != 0)
		{
			memcpy(get_msg(sending_buffer), &(command->msg[byteOffset]), sizeof(uint8_t) * payload_len);
			// Add the null terminator
			sending_buffer[cast_len + payload_len-1] = '\0';
					// Send it off
			sent = safeSendPDU(socketNum, sending_buffer, cast_len+payload_len);
		} else
		{
			// Adding a blank to be recieved on the on the other end
			get_msg(sending_buffer)[0] = '\0';
			sent = safeSendPDU(socketNum, sending_buffer, cast_len+payload_len+1);
		}
		return sent;
}

/* 	Add destination handles to the buffer, starting at the top.
	Assumes that it can write anywhere in the buffer.
	Returns the amount of bytes added. */
int addDestHandles(Command *command, uint8_t *out_buf)
{
	uint8_t num_handles = command->destsLen;
	int out_buf_len = 0;
	// Append the number of destinations to the output
	memcpy(get_num_dests(out_buf), &num_handles, sizeof(uint8_t));
	out_buf_len ++;
	// Append each handle
	for (int i = 0; i < num_handles; i++)
	{
		// Copy over the length of the handle
		memcpy(get_nth_dest_len(out_buf, i), &(command->lengths[i]), sizeof(uint8_t));
		// Copy over the handle
		memcpy(get_nth_dest(out_buf, i), command->dests[i], sizeof(uint8_t) * command->lengths[i]);
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
		handleMsg(buffer, recvBytes);
	}
	else
	{
		printf("Server Terminated\n");
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
		printErrorMsg(packet, len);
	}
	// Chat message
	else if (flag == PDU_UNICAST || flag == PDU_MULTICAST || flag == PDU_BROADCAST)
	{
		printChat(packet, len);
	}
	// Handle message
	else if (flag == PDU_H_LEN)
	{
		// Enter flag mode
		receive_handles(packet, len);
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

int sender_name_toStr(uint8_t *packet, uint8_t *dest)
{
	uint8_t sender_len = *(get_src_len(packet));
	memcpy(dest, get_src(packet), sizeof(uint8_t) * sender_len);
	dest[sender_len] = '\0';
	return sender_len;
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

	// Checking for a buffer overflow
	if (aChar != '\n')
	{
		printf("Message too long\n");
		// Dump the rest
		while (getchar() != '\n');
		return -1;
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
	while (response != PDU_ACCEPT) response = getConnectResp(socketNum, handleName);
}

// Getting the connection response from the server
int getConnectResp(int socketNum, char* handleName)
{
	// int recvBytes = 0;
	uint8_t buffer[MAXBUF];   //data buffer

	safeRecvPDU(socketNum, buffer, MAXBUF);
	uint8_t serverResp = buffer[0];

	// 3? stop program
	if (serverResp == PDU_REJECT)
	{
		printf("Handle already in use: \n");
		exit(3);
	}
	// 2? allow connection
	else return serverResp;
}

// Handle code
void receive_handles()
{
	// The program will enter a mode that waits for the packets
	// Wait for the server to respond
	int recvBytes = 0;
	uint8_t buffer[MAXBUF];   //data buffer

	recvBytes = safeRecvPDU(socketNum, buffer, MAXBUF);
	// Print the length of the handle list
	print_htable_len(buffer);

	uint8_t recv_Flag = 0;    
	// block until the packets  are recieved
	while(recv_Flag != PDU_H_TERM)
	{
		recvBytes = safeRecvPDU(socketNum, buffer, MAXBUF);
		recv_Flag = parse_h_msg(buffer, recvBytes);
	}
}

void print_htable_len(uint8_t *packet)
{
	uint32_t h_list_len_n = 0;
	memcpy(&h_list_len_n, &(packet[1]), sizeof(uint32_t));
	uint32_t h_list_len = ntohl(h_list_len_n);
	printf("Number of clients: %u\n", h_list_len);
}

uint8_t parse_h_msg(uint8_t *buffer, int buffer_len)
{
	uint8_t flag;
	memcpy(&flag, buffer, sizeof(uint8_t));
	if (flag == PDU_H_TERM) return flag;
	else if (flag == PDU_H_RESP) print_handle(buffer);
	return flag;
}

void print_handle(uint8_t *buffer)
{
	uint8_t h_len = buffer[1];
	uint8_t *handle = &(buffer[2]);
	uint8_t handle_str[h_len+1];
	memcpy(handle_str, handle, sizeof(uint8_t) * h_len);
	handle_str[h_len] = '\0';
	printf("\t%s\n", handle_str);
}

void printErrorMsg(uint8_t *packet, int len)
{
	uint8_t handleLen = *(get_src_len(packet));
	uint8_t handle[handleLen+1];
	memcpy(handle, get_src(packet), sizeof(uint8_t) * handleLen);
	handle[handleLen] = '\0';

	printf("\nClient with handle %s does not exist\n", handle);
}
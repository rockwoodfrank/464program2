/******************************************************************************
* serverPoll.c
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
#include "pduFlags.h"
#include "safePDU.h"
#include "handleTable.h"
#include "getPDUdata.h"

#include "printBytes.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
void serverControl(int portNumber);
void addNewSocket(int mainServerSocket);
void processClient(int clientSocket);
void handle_msg(uint8_t *packet, int packetLen, int send_sock);
void processPacket(uint8_t *packet, int packetLen, int recv_sock);
void connect_Client(char *conn_handle, int recv_sock);
void route_cast(uint8_t* packet, int packetLen);
int get_handles(uint8_t *packet, int *socks);

int main(int argc, char *argv[])
{
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	serverControl(portNumber);
	
	// /* close the sockets */
	// close(mainServerSocket);

	
	return 0;
}

void serverControl(int portNumber)
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);
	
	// add main server socket to poll set.
	setupPollSet();
	addToPollSet(mainServerSocket);

	// Creating the handle table
	init_handle_table();

	// While (1)
	while (1)
	{
		// Call poll()
		int readysocket = pollCall(-1);
		// If poll() returns the main server socket, call addNewSocket()
		if (readysocket == mainServerSocket)
		{
			addNewSocket(mainServerSocket);
		}
		// If poll() returns a client socket, call processClient()
		else if (readysocket != -1)
		{
			processClient(readysocket);
		}
	}
}

void addNewSocket(int mainServerSocket)
{
	int clientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);
	addToPollSet(clientSocket);
}

void processClient(int clientSocket)
{
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the client_socket
	messageLen = safeRecvPDU(clientSocket, dataBuffer, MAXBUF);
	if (messageLen > 0)
	{
		processPacket(dataBuffer, messageLen, clientSocket);
	}
	else
	{
		printf("Socket %d: Connection closed by other side\n", clientSocket);
		removeFromPollSet(clientSocket);
		close(clientSocket);
	}
}

void processPacket(uint8_t *packet, int packetLen, int recv_sock)
{
	// TODO: Error checking
	uint8_t conn_handle[MAX_HANDLE_LENGTH];
	uint8_t flag = *(get_flag(packet));
	uint8_t handle_len = *(get_src_len(packet));
	memcpy(conn_handle, get_src(packet), handle_len);
	// Append a null character
	conn_handle[handle_len] = '\0';
	// A switch case as for what to do next
	if (flag == PDU_CONNECT)
	{
		connect_Client(conn_handle, recv_sock);
	}
	else if (flag == PDU_BROADCAST)
	{
		//
	}
	else if (flag == PDU_HANDLES)
	{
		//
	}
	else if (flag == PDU_UNICAST || flag == PDU_MULTICAST)
	{
		route_cast(packet, packetLen);
	}
	else
	{
		// Throw an error
		// Quit the server?
	}
	// printf("Socket %d: Message received, length: %d, Flag: %hhu Data: ", clientSocket, messageLen, flag);
	// printBytes(dataBuffer, messageLen);
	
	// messageLen = sendPDU(clientSocket, &new_data_buffer, 1);
	// printf("Socket %d: msg sent: %d bytes, text: %s\n", clientSocket, messageLen, &new_data_buffer);
}

void connect_Client(char *conn_handle, int recv_sock)
{
	printf("Connecting a new client with handle %s\n", conn_handle);
	uint8_t response;
	// Connection code
	// make sure the handle isn't already in the table
	if (lookup_handle_byname(conn_handle) != -1)
	{
		response = PDU_REJECT;
		printf("Client attempted to connect with duplicate handle.\n");
		uint8_t response = PDU_REJECT;
		sendPDU(recv_sock, &response, 1);
	}
	// Adding to handle table
	else
	{
		add_handle(conn_handle, recv_sock);
		response = PDU_ACCEPT;
		sendPDU(recv_sock, &response, 1);
	}
}

void route_cast(uint8_t* packet, int packetLen)
{
	int send_socks[9];
	int num_socks;
	// Get the handles that we're sending to
	num_socks = get_handles(packet, send_socks);
	printf("Recieved");
	printBytes(packet, packetLen);

	// TODO: Check that they're real

	// For each handle, send the packet
	for (int i = 0; i < num_socks; i++)
		sendPDU(send_socks[i], packet, packetLen);
}

int get_handles(uint8_t *packet, int *socks)
{
	int num_handles = *(get_num_dests(packet));
	for (int i = 0; i < num_handles; i++)
	{
		int h_len = *(get_nth_dest_len(packet, i));
		uint8_t h_name[MAX_HANDLE_LENGTH];
		memcpy(h_name, get_nth_dest(packet, i), h_len);
		// Add a null terminator
		h_name[h_len] = '\0';
		socks[i] = lookup_handle_byname(h_name);
		if (socks[i] == -1) return -1;
	}
	return num_handles;
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}


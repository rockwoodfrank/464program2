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
void route_cast(int src_sock, uint8_t* packet, int packetLen);
int get_handles(int src_sock, uint8_t *packet, int *socks);

// Handle sending
void send_handles(int socketNum);
int send_num_handles(int socket, uint32_t num_handles);
int send_handle(int socket, int index);

// Broadcasting
void broadcast_msg(int send_sock, uint8_t *packet, int packetLen);

// Error sending
void send_error(int src_sock, uint8_t *bad_hname, uint8_t hname_len);

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
		// printf("Socket %d: Connection closed by other side\n", clientSocket);
		remove_handle(clientSocket);
		removeFromPollSet(clientSocket);
		close(clientSocket);
	}
}

void processPacket(uint8_t *packet, int packetLen, int recv_sock)
{
	uint8_t conn_handle[MAX_HANDLE_LENGTH];
	uint8_t flag = *(get_flag(packet));
	uint8_t handle_len = *(get_src_len(packet));
	memcpy(conn_handle, get_src(packet), handle_len);
	// Append a null character
	conn_handle[handle_len] = '\0';
	// A switch case as for what to do next
	if (flag == PDU_CONNECT)
	{
		connect_Client((char *)conn_handle, recv_sock);
	}
	else if (flag == PDU_BROADCAST)
	{
		broadcast_msg(recv_sock, packet, packetLen);
	}
	else if (flag == PDU_HANDLES)
	{
		send_handles(recv_sock);
	}
	else if (flag == PDU_UNICAST || flag == PDU_MULTICAST)
	{
		route_cast(recv_sock, packet, packetLen);
	}
	else
	{
		// Throw an error
		// Quit the server?
	}
}

void connect_Client(char *conn_handle, int recv_sock)
{
	uint8_t response;
	// Connection code
	// make sure the handle isn't already in the table
	if (lookup_handle_byname(conn_handle) != -1)
	{
		response = PDU_REJECT;
		printf("Client attempted to connect with duplicate handle.\n");
		uint8_t response = PDU_REJECT;
		safeSendPDU(recv_sock, &response, 1);
	}
	// Adding to handle table
	else
	{
		add_handle(conn_handle, recv_sock);
		response = PDU_ACCEPT;
		safeSendPDU(recv_sock, &response, 1);
	}
}

void route_cast(int src_sock, uint8_t* packet, int packetLen)
{
	int send_socks[9];
	int num_socks;
	// Get the handles that we're sending to
	num_socks = get_handles(src_sock, packet, send_socks);

	// For each handle, send the packet
	for (int i = 0; i < num_socks; i++)
		if (send_socks[i] != -1)
			safeSendPDU(send_socks[i], packet, packetLen);

}

void send_handles(int socketNum)
{
	// Get number of handles
	uint32_t num_handles = get_num_handles();
	// Respond with number of handles
	send_num_handles(socketNum, num_handles);

	// Sending each handle
	int socks[num_handles];
	get_handles_from_table(&socks);
	for (int i = 0; i < num_handles; i++)
		send_handle(socketNum, socks[i]);

	// Sending the terminating signal
	uint8_t term = PDU_H_TERM;
	safeSendPDU(socketNum, &term, 1);
}

int send_num_handles(int socket, uint32_t num_handles)
{
	int sent = 0;
	uint8_t packet[PDU_H_PACKET_LEN];
	packet[0] = PDU_H_LEN;
	uint32_t socket_n = htonl(num_handles);
	memcpy(&(packet[1]), &socket_n, sizeof(uint32_t));
	sent = safeSendPDU(socket, packet, PDU_H_PACKET_LEN);
	return sent;
}

int send_handle(int send_socket, int h_socket)
{
	char *handle = lookup_handle_bysock(h_socket);
	uint8_t h_len = strlen(handle);
	uint8_t send_buffer[1+h_len];
	int sent = 0;
	// Adding the flag
	send_buffer[0] = PDU_H_RESP;

	// Adding the handle length
	memcpy(&(send_buffer[1]), &h_len, sizeof(uint8_t));

	// Adding the handle
	memcpy(&(send_buffer[2]), handle, sizeof(uint8_t) * h_len);

	sent = safeSendPDU(send_socket, send_buffer, h_len+2);
	return sent; 
}

int get_handles(int src_sock, uint8_t *packet, int *socks)
{
	int num_handles = *(get_num_dests(packet));
	for (int i = 0; i < num_handles; i++)
	{
		int h_len = *(get_nth_dest_len(packet, i));
		uint8_t h_name[MAX_HANDLE_LENGTH];
		memcpy(h_name, get_nth_dest(packet, i), h_len);
		// Add a null terminator
		h_name[h_len] = '\0';

		int sock_ret = lookup_handle_byname((char *)h_name);
		if (sock_ret == -1) 
		{
			send_error(src_sock, h_name, h_len);
			socks[i] = -1;
		}
		else socks[i] = sock_ret;
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

void broadcast_msg(int send_sock, uint8_t *packet, int packetLen)
{
	// Get all of the the handles
	uint32_t num_handles = get_num_handles();
	int socks[num_handles];
	get_handles_from_table(&socks);
	// Cycle through the handles and send the message
	for (int i = 0; i<num_handles; i++)
		if (send_sock != socks[i])
			safeSendPDU(socks[i], packet, packetLen);
}

void send_error(int src_sock, uint8_t *bad_hname, uint8_t hname_len)
{
	uint8_t buffer[hname_len+2];
	// Add the flag
	buffer[0] = PDU_ERROR;
	buffer[1] = hname_len;
	memcpy(&(buffer[2]), bad_hname, sizeof(uint8_t) * hname_len);
	safeSendPDU(src_sock, buffer, hname_len+2);
}
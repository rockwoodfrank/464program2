#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "pdu.h"

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData)
{
    // Generating the bytes to send
    uint16_t pdu_len_h = lengthOfData;
    uint16_t pdu_len_n = htons(pdu_len_h);

    // Generating the packet
    uint8_t send_buf[lengthOfData + 2];
    memcpy(send_buf, (uint8_t *) &pdu_len_n, sizeof(uint16_t));
    memcpy(&(send_buf[2]), dataBuffer, lengthOfData);

    // Sending the packet on the socket
    int bytesSent = send(clientSocket, send_buf, lengthOfData + 2, 0);
    if (bytesSent < 0)
    {
        perror("Send PDU error");
        exit(-1);
    }

    return bytesSent - 2;
}

int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize)
{
    // Getting the pdu 
    uint16_t pdu_len_n;
    int lenR = recv(socketNumber, (uint8_t *)&pdu_len_n, sizeof(uint16_t), MSG_WAITALL);
   
    // Error checking and closed connections
    if (lenR == 0 || (lenR == -1 && errno == ECONNRESET))
    {
        return 0;
    } 
    else if (lenR < 0)
    {
        perror("recv error");
        exit(-1);
    }

    uint16_t pdu_len_h = ntohs(pdu_len_n);

    // Checking that the buffer is large enough to store the data
    if (bufferSize < pdu_len_h)
    {
        perror("Buffer too small");
        exit(-1);
    }

    // Getting the pdu data
    int bytesReceived = recv(socketNumber, dataBuffer, pdu_len_h, MSG_WAITALL);
    // Error checking and closed connections
    if (bytesReceived == 0 || (bytesReceived == -1 && errno == ECONNRESET))
    {
        return 0;
    } else if (bytesReceived < 0)
    {
        perror("recv error");
        exit(-1);
    }
 
    return bytesReceived;
}
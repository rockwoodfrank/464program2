
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
#include <errno.h>

#include "networks.h"
#include "safePDU.h"
#include "pdu.h"

int safeRecvPDU(int socketNum, uint8_t * buffer, int bufferLen)
{
    int bytesReceived = recvPDU(socketNum, buffer, bufferLen);
    if (bytesReceived < 0)
    {
        if (errno == ECONNRESET)
        {
            bytesReceived = 0;
        }
        else
        {
            perror("recv call");
            exit(-1);
        }
    }
    return bytesReceived ;
}

int safeSendPDU(int socketNum, uint8_t * buffer, int bufferLen)
{
	int bytesSent = 0;
	if ((bytesSent = sendPDU(socketNum, buffer, bufferLen)) < 0)
	{
        perror("send call");
       exit(-1);
     }
	 
    return bytesSent;
}
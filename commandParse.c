#include "commandParse.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "printBytes.h"

uint8_t determine_flag(uint8_t* userInput, int inputLen);
int parseDestsLen(uint8_t *buffer, int buffer_len);

// Parse the command and place it in the passed struct
int commandParse(uint8_t *buffer, int buffer_len, Command *command)
{
    command->flag = determine_flag(buffer, buffer_len);
    if (command->flag == 0) return -1;
    else if (command->flag == PDU_HANDLES) return 0;
    // Verifying the length to prevent a segfault
    if ((buffer_len < 5) && command->flag != PDU_BROADCAST)
    {
        printf("Invalid command format\n");
        return -1;
    }
    if (command->flag == PDU_MULTICAST || command->flag == PDU_UNICAST)
    {
        if (command->flag == PDU_MULTICAST)
        {
            command->destsLen = parseDestsLen(buffer, buffer_len);
            // Verify a number was entered
            if(command->destsLen < 1 || command->destsLen > 9)
            {
                printf("Enter a number of destinations between 1 and 9.\n");
                return -1;
            }
        } else command->destsLen = 1;
        // grab each dest
        for (int i = 0; i<(command->destsLen); i++)
        {
            // Generate a new char array for each so they can be treated like strings
            command->dests[i] = strtok(NULL, " ");
            if (command->dests == NULL)
            {
                printf("Mismatch between number and destinations\n");
                return -1;
            }
            command->lengths[i] = strlen((char *)command->dests[i]);
            if (command->lengths[i] > 100)
            {
                printf("Invalid handle, handle longer than 100 characters: %s\n", command->dests[i]);
                return -1;
            }
        }
    }
    // Storing the message
    // Readpos should be at the beginning of the message segment, so we'll use it to keep track
    // Of message data

    command->msg = strtok(NULL, "\n");
    // Checking for empty messages
    if (command->msg == NULL) command->msgLen = 0;
    else command->msgLen = strlen(command->msg) + 1;
    return 0;
}

// Parse the user's input and figure out what to do next
uint8_t determine_flag(uint8_t* userInput, int inputLen)
{
	// Verify the input is at least 3(%x + null) characters
	// and verify the input begins with a percent sign
	if (inputLen < 3 || userInput[0] != '%')
	{
		printf("Invalid command\n");
		return 0;
	}
    char *flag = strtok((char*)userInput, " ");
	// %M - Messsages
	if ((strcmp(flag, "%M") == 0) ||(strcmp(flag, "%m") == 0))
		return PDU_UNICAST;
	// %B - Broadcast
	else if ((strcmp(flag, "%B") == 0)|| (strcmp(flag, "%b") == 0))
		return PDU_BROADCAST;
	// %C - Multicast
	else if ((strcmp(flag, "%C") == 0)|| (strcmp(flag, "%c") == 0))
		return PDU_MULTICAST;
	// %L - List
	else if ((strcmp(flag, "%L") == 0) || (strcmp(flag, "%l") == 0))
		return PDU_HANDLES;
	else 
	{
		printf("Invalid command\n");
		return 0;
	}
}

int parseDestsLen(uint8_t *buffer, int buffer_len)
{
    // The number will be 4th item
    int numDests = strtok(NULL, " ")[0] - '0';
    return numDests;
}
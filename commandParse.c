#include "commandParse.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "printBytes.h"

uint8_t determine_flag(uint8_t* userInput, int inputLen);
int parseDestsLen(uint8_t *buffer, int buffer_len);
uint8_t get_word_length(uint8_t *src);

// Parse the command and place it in the passed struct
int commandParse(uint8_t *buffer, int buffer_len, Command *command)
{
    int read_pos = 0;
    command->flag = determine_flag(buffer, buffer_len);
    if (command->flag == 0) return -1;
    else if (command->flag == PDU_HANDLES) return 0;
    if (command->flag == PDU_MULTICAST)
    {
        command->destsLen = parseDestsLen(buffer, buffer_len);
        // grab each dest
        read_pos = 5;
        for (int i = 0; i<(command->destsLen); i++)
        {
            uint8_t *buf = &(buffer[read_pos]);
            uint8_t h_len = get_word_length(buf);
            // Generate a new char array for each so they can be treated like strings
            command->dests[i] = buf;
            command->lengths[i]=h_len;
            read_pos += h_len + 1;
        }
    } else if (command->flag == PDU_UNICAST)
    {
        command->destsLen = 1;
        read_pos = 3;
        command->dests[0] = &(buffer[read_pos]);
        command->lengths[0] = get_word_length(command->dests[0]);
        read_pos += command->lengths[0] + 1;
    }
    // Storing the message
    // Readpos should be at the beginning of the message segment, so we'll use it to keep track
    // Of message data
    command->msg = (uint8_t *)&(buffer[read_pos]);
    // Might need to add a +1 in there
    command->msgLen = buffer_len - read_pos;
    return 0;
}

// Parse the user's input and figure out what to do next
uint8_t determine_flag(uint8_t* userInput, int inputLen)
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

int parseDestsLen(uint8_t *buffer, int buffer_len)
{
    // TODO: error checing
    // The number will be 4th item
    int numDests = buffer[3] - '0';
    return numDests;
}

// Using iu8s because the word length shouldn't be over 100
uint8_t get_word_length(uint8_t *src)
{
    uint8_t i = 0;
    // TODO: Error on word length greater than 100
	while(!isspace(src[i]) && src[i] != 0) i++;
    return i;
}
#ifndef __COMMAND_PARSE__
#define __COMMAND_PARSE__

#include "pduFlags.h"
#include <stdint.h>

typedef struct Command {
    uint8_t flag;
    uint8_t *dests[9];
    uint8_t lengths[9];
    int destsLen;
    uint8_t *msg;
    int msgLen;
} Command;

int commandParse(char *buffer, int buffer_len, Command *command);

#endif
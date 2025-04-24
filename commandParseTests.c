#include "commandParse.h"
#include <assert.h>
#include <stdio.h>
#include "printBytes.h"

int main(int argc, char *argv[])
{
    Command newCommand;
    char userInput[33] = "%c 2 client1 client2 hello world";
    printBytes(userInput, 33);
    commandParse(userInput, 33, &newCommand);
    assert(newCommand.flag == PDU_MULTICAST);
    assert(newCommand.destsLen == 2);
    printBytes(newCommand.msg, newCommand.msgLen);
    return 0;
}
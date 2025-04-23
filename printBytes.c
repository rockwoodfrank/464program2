// Developed by Rockwood Frank for Debugging Purposes
#include "printBytes.h"

void printBytes(uint8_t *stream, int len)
{
    for (int i = 0; i < len; i++)
    {
        printf("%02hhx", stream[i]);
        if (stream[i] >= 32 && stream[i] <= 127)
            printf("(%c)", stream[i]);
        printf(" ");
    }
    printf("\n");
}
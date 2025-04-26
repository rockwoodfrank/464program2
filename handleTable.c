#include "handleTable.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

// The server needs to store a pointer to the handle table.
char**  names;
int*    sockets;
int     tableSize;

void grow_handle_table();

// Initializing the table
void init_handle_table()
{
    tableSize = DEFAULT_TABLE_SIZE;
    names = malloc(sizeof(char *) * tableSize);
    if (names == NULL)
    {
        perror("htable");
        exit(-1);
    }
    sockets = malloc(sizeof(int) * tableSize);
    if (sockets == NULL)
    {
        perror("htable");
        exit(-1);
    }
    // Clear out the arrays because of the search
    memset(names, 0, sizeof(char *) * tableSize);
    // A zero in the socket means it does not exist
    memset(sockets, 0, sizeof(int) * tableSize);
}

void remove_handle_table()
{
    free(names);
    free(sockets);
}

/*  
    Gets the corresponding socket for a handle name.
    returns a -1 if the socket doesn't exist
*/
int lookup_handle_byname(char *name)
{
    // Linear search
    for (int i = 0; i < tableSize; i++)
        if (names[i] != NULL && strcmp(name, names[i]) == 0)
            return sockets[i];
    return -1;
}

// Handling lookup by socket number
char* lookup_handle_bysock(int sock)
{
    // Linear search
    for (int i = 0; i < tableSize; i++)
        if (sockets[i] == sock)
            return names[i];
    return NULL;
}


// Add a value to the handle table. Returns 0 on success.
// Will grow the handle table if too many values.
int add_handle(char* name, int socketNum)
{
    int tablePos = -1;
    // Find the next free entry
    for (int i = 0; i<tableSize; i++)
        if (sockets[i] == 0)
            tablePos = i;
    
    if (tablePos == -1)
    {
        // we need to realloc the array
        tablePos = tableSize;
        grow_handle_table();
    }
    
    char *newName = malloc(sizeof(char) * MAX_HANDLE_LENGTH);
    if (newName == NULL)
    {
        perror("add htable");
        exit(-1);
    }
    strcpy(newName, name);
    names[tablePos] = newName;
    sockets[tablePos] = socketNum;
    
    return 0;
}

int remove_handle(int socketNum)
{
    int h_index = -1;
    for (int i = 0; i < tableSize; i++)
        if (sockets[i] == socketNum)
            h_index = i;
    if (h_index == -1) return -1;
    free(names[h_index]);
    names[h_index] = NULL;
    sockets[h_index] = 0;

    return h_index;
}

// Returns the number of handles in the table
u_int32_t get_num_handles()
{
    u_int32_t num_handles = 0;
    for (int i = 0; i<tableSize; i++)
    {
        if (sockets[i] != 0)
            num_handles++;
    }
    return num_handles;
}

int get_handles_from_table(int *socks)
{
    int iter = 0;
    for (int i = 0; i<tableSize; i++)
    {
        if (sockets[i] != 0)
            socks[iter++] = sockets[i];
    }
    return 0;
}

void grow_handle_table()
{
    int newTableSize = tableSize *2;
    if ((names = realloc(names, sizeof(char *) * newTableSize)) == NULL)
    {
        perror("realloc");
        exit(-1);
    }
    // Setting the new values to zero
    memset(&(names[tableSize]), 0, sizeof(char *) * tableSize);
    if ((sockets = realloc(sockets, sizeof(int) * newTableSize)) == NULL)
    {
        perror("realloc");
        exit(-1);
    }
    memset(&(sockets[tableSize]), 0, sizeof(int) * tableSize);
    tableSize = newTableSize;
}
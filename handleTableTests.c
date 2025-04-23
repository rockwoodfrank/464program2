#include "handleTable.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
    init_handle_table();

    add_handle("hello", 4);
    add_handle("world", 5);

    assert(strcmp(lookup_handle_bysock(5), "world") == 0);
    assert(lookup_handle_byname("hello") == 4);
    assert(lookup_handle_byname("bananas") == -1);
    assert(remove_handle(5) >= 0);
    assert(lookup_handle_byname("world") == -1);
    remove_handle(4);

    for (int i=0; i<300; i++)
    {
        char handleName[15];
        sprintf(handleName, "handle%d", i);
        add_handle(handleName, i);
    }

    assert(lookup_handle_byname("handle278") == 278);

    return 0;
}
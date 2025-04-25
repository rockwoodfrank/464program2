#ifndef __H_TABLE__
#define __H_TABLE__

#define DEFAULT_TABLE_SIZE 32
#define MAX_HANDLE_LENGTH 100

#include <stdint.h>

void init_handle_table();
void remove_handle_table();
int lookup_handle_byname(char *name);
char* lookup_handle_bysock(int sock);
int add_handle(char* name, int socketNum);
int remove_handle(int socketNum);
uint32_t get_num_handles();
// Get all handles and append them to the socks array
int get_handles_from_table(int *socks);

#endif
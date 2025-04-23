#ifndef __H_TABLE__
#define __H_TABLE__

#define DEFAULT_TABLE_SIZE 32
#define MAX_HANDLE_LENGTH 100

void init_handle_table();
void remove_handle_table();
int lookup_handle_byname(char *name);
char* lookup_handle_bysock(int sock);
int add_handle(char* name, int socketNum);
int remove_handle(int socketNum);

#endif
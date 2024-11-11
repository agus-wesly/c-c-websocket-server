#ifndef MEM_H
#define MEM_H

#include <stdio.h>

void *create_shared_memory(size_t size);
void add_new_client(char *clients, int new_socket);

#endif // !MEM_H

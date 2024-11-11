#include <string.h>
#include <sys/mman.h>

void *create_shared_memory(size_t size)
{
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED | MAP_ANONYMOUS;
    return mmap(NULL, size, protection, visibility, -1, 0);
}

void add_new_client(char *clients, int new_socket)
{
    size_t size = strlen(clients);
    clients[size] = new_socket;
}

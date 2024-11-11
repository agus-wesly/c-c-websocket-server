#include "mem.h"
#include "ws.h"
#include <asm-generic/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int PORT = 5555;
const int MAX_PAYLOAD_SIZE = 1024;
const int MAX_CLIENT = 100;
const char *HEADER = "HTTP/1.1 101 Switching Protocols\r\n";

typedef struct
{
    int new_socket;
} thread_info;

typedef struct
{
    size_t curr;
    thread_info *threads;
} thread_args;

int do_handshake(char *buffer, thread_info *curr_thread)
{
    // Process the key from client
    char *copy = (char *)malloc(strlen(buffer) + 1);
    char *websocket_key = get_websocket_key(buffer, copy);
    char *extracted_key = extract_key(websocket_key);
    if ((write_header(curr_thread->new_socket, extracted_key, HEADER)) == -1)
    {
        return -1;
    }
    printf("New Client : %i\n", curr_thread->new_socket);
    free(copy);
    return 0;
}

void *establish_connection(void *args)
{
    thread_args *th_args = (thread_args *)args;
    thread_info *curr_thread = &(th_args->threads[th_args->curr]);
    thread_info *threads = th_args->threads;
    char buffer[16000] = {0};
    long resp = recv(curr_thread->new_socket, &buffer, 16000, 0);

    if ((do_handshake(buffer, curr_thread) < 0))
    {
        perror("On handshake");
        exit(EXIT_FAILURE);
    };

    char buffer_ws[16000] = {0};
    while (1)
    {
        if ((handle_received_message(curr_thread->new_socket, buffer_ws) < 0))
        {
            perror("On receiving message");
            exit(EXIT_FAILURE);
        };
        char *ws_message = (char *)malloc(MAX_PAYLOAD_SIZE * sizeof(char));
        if ((decode_websocket_message(buffer_ws, ws_message)) < 0)
        {
            perror("On receiving message");
            exit(EXIT_FAILURE);
        };
        printf("msg : %s\n", ws_message);
        // Broadcast it !
        for (int i = 0; i < MAX_CLIENT; ++i)
        {
            if (threads[i].new_socket == -1 || threads[i].new_socket == curr_thread->new_socket)
            {
                continue;
            }
            printf("SENDING TO : %i FROM :  %i\n", threads[i].new_socket, curr_thread->new_socket);
            if ((send_reply_message(threads[i].new_socket, ws_message)) < 0)
            {
                perror("On sending reply");
                break;
            };
        }
        // Send to ourself
        if ((send_reply_message(curr_thread->new_socket, ws_message)) < 0)
        {
            perror("On sending reply");
            break;
        };
        free(ws_message);
    }
    return args;
}

int main()
{
    thread_info threads[MAX_CLIENT];
    char *clients = (char *)create_shared_memory(MAX_CLIENT);
    int socket_fd;
    int new_socket;
    int pid;
    struct addrinfo address, *res, *p;
    struct sockaddr new_addr;
    memset(&address, 0, sizeof(address));
    address.ai_family = AF_UNSPEC;
    address.ai_socktype = SOCK_STREAM;
    address.ai_flags = AI_PASSIVE;

    memset(threads, -1, sizeof(threads));

    if (getaddrinfo(NULL, "5555", &address, &res) != 0)
    {
        perror("ERROR ON FILLING ADDR INFO");
        return 1;
    };

    for (p = res; p != NULL; p = p->ai_next)
    {
        if ((socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("On create socket");
            continue;
        }
        int yes = 1;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        {
            perror("On setting socket option");
            continue;
        }
        if (bind(socket_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("On bind");
            continue;
        };
        break;
    }
    freeaddrinfo(res);
    // listen
    if (listen(socket_fd, 10) < 0)
    {
        perror("On listen");
        exit(EXIT_FAILURE);
    };
    printf("+++++++++++++++++++++++++++++Waiting for a new connection+++++++++++++++++++++++++++++\n");
    size_t i;
    while (1)
    {
        int size_new_addr = sizeof(new_addr);
        if ((new_socket = accept(socket_fd, (struct sockaddr *)&new_addr, (socklen_t *)&size_new_addr)) < 0)
        {
            perror("On accept");
            exit(EXIT_FAILURE);
        };

        for (i = 0; i < MAX_CLIENT; ++i)
        {
            if (threads[i].new_socket == -1)
            {
                threads[i].new_socket = new_socket;
                break;
            }
        }
        pthread_t new_thread;
        thread_args arg = {i, threads};
        pthread_create(&new_thread, NULL, &establish_connection, &arg);
        pthread_detach(new_thread);
    }
    close(socket_fd);
    return 0;
}

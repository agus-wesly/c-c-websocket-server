#include "ws.h"
#include <asm-generic/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT "5555"
#define MAX_EVENTS 16

const char *HEADER = "HTTP/1.1 101 Switching Protocols\r\n";

int do_handshake(int current_fd, char *buffer)
{
    read(current_fd, buffer, 1024);
    char *websocket_key = get_websocket_key(buffer);
    char *extracted_key = extract_key(websocket_key);
    if ((write_header(current_fd, extracted_key, HEADER)) == -1)
    {
        perror("While send header");
        return -1;
    }
    return 0;
}

ws_frame_data *socket_clients[MAX_CLIENT];

ws_frame_data *is_socket_client(int fd)
{
    for (int i = 0; i < 100; ++i)
    {
        if (socket_clients[i] != 0 && socket_clients[i]->fd == fd)
            return socket_clients[i];
    }
    return 0;
}
int socket_fd;

void set_non_blocking(int sock)
{
    int opt;

    opt = fcntl(sock, F_GETFL);
    if (opt < 0)
    {
        printf("fcntl(F_GETFL) fail.");
    }
    opt |= O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opt) < 0)
    {
        printf("fcntl(F_SETFL) fail.");
    }
}

int main()
{
    int new_socket;
    int pid;
    struct addrinfo address, *res, *p;
    struct sockaddr new_addr;
    struct epoll_event evt;
    struct epoll_event polled_events[MAX_EVENTS];

    memset(&address, 0, sizeof(address));
    address.ai_family = AF_UNSPEC;
    address.ai_socktype = SOCK_STREAM;
    address.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &address, &res) != 0)
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
    if (listen(socket_fd, 10) < 0)
    {
        perror("On listen");
        exit(EXIT_FAILURE);
    };

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    evt.events = EPOLLIN;
    evt.data.fd = socket_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &evt) == -1)
    {
        perror("ERROR IN EPOLL CONTROL");
        exit(EXIT_FAILURE);
    }

    printf("+++++++++++++++++++++++++++++Waiting for a new connection+++++++++++++++++++++++++++++\n");
    for (;;)
    {
        int nfds = epoll_wait(epoll_fd, polled_events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; ++i)
        {
            int current_fd = polled_events[i].data.fd;
            if (current_fd == socket_fd)
            {
                int size_new_addr = sizeof(new_addr);
                if ((new_socket = accept(socket_fd, (struct sockaddr *)&new_addr, (socklen_t *)&size_new_addr)) < 0)
                {
                    perror("On accept");
                    exit(EXIT_FAILURE);
                };
                evt.events = EPOLLIN;
                evt.data.fd = new_socket;
                if ((epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &evt)) == -1)
                {
                    perror("epoll_ctl");
                    exit(EXIT_FAILURE);
                }
            }
            else if (is_socket_client(current_fd) != 0)
            {
                ws_frame_data *wfd = is_socket_client(current_fd);
                if (process_message(wfd) == -1)
                {
                    free(wfd);
                    continue;
                };
            }
            else
            {
                printf("Receiving data from the client with socket fd: %d\n", current_fd);
                char *buffer = (char *)calloc(1024, sizeof(char));
                if (do_handshake(current_fd, buffer) == -1)
                {
                    free(buffer);
                    continue;
                };
                set_non_blocking(current_fd);
                for (int i = 0; i < MAX_CLIENT; ++i)
                {
                    if (socket_clients[i] == 0)
                    {
                        ws_frame_data *new_ws = (ws_frame_data *)calloc(1, sizeof(ws_frame_data));
                        new_ws->fd = current_fd;
                        socket_clients[i] = new_ws;
                        break;
                    }
                    // TODO: handle max connection
                }
                printf("New Client : %i\n", current_fd);
                free(buffer);
            }
        }
    }
    close(socket_fd);
    return 0;
}

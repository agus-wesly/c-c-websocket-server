#include <bits/types/struct_iovec.h>

#ifndef WS_H
#define WS_H
#define MAX_PAYLOAD_SIZE 1024

#define MAX_CLIENT 100
typedef struct
{
    int fd;
    unsigned char buffer_ws[1024];
    size_t cur_pos;
    int amt_read;
} ws_frame_data;

extern ws_frame_data *socket_clients[MAX_CLIENT];

extern int socket_fd;

char *get_websocket_key(char *buffer);
char *extract_key(char *websocket_key);
int decode_websocket_message(unsigned char *message, unsigned char **ws_message);
int send_reply_message(int fd, char *msg);
int write_header(int new_socket, char *extracted_key, const char *HEADER);
int handle_received_message(int current_fd, unsigned char *buffer_ws);
int Base64Encode(const char *message, char **buffer);
int process_message(ws_frame_data *wfd);

#endif // !WS_H

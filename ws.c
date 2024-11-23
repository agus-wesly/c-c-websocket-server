#include "ws.h"
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// 9
// 0b00001001
// 0b00000001
void print_bits(unsigned char byte)
{
    for (int i = 7; i >= 0; i--)
    {
        printf("%d", (byte >> i) & 1);
    }
    printf("\n");
}

char *get_websocket_key(char *buffer)
{
    char *token = strtok(buffer, "\r\n");
    char *websocket_key = NULL;
    while (token != NULL)
    {

        if (strstr(token, "Sec-WebSocket-Key:") != NULL)
        {
            websocket_key = strtok(token, ": ");
            websocket_key = strtok(NULL, ": ");
        }
        token = strtok(NULL, "\r\n");
    }
    return websocket_key;
}

char *extract_key(char *websocket_key)
{
    size_t offset;
    char *hexresult = (char *)malloc(41);
    memset(hexresult, '\0', 41);
    const char *MAGIC_STRING = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    websocket_key = strcat(websocket_key, MAGIC_STRING);
    SHA1((unsigned char *)websocket_key, strlen(websocket_key), (unsigned char *)hexresult);
    // Encode to base64
    char *base64EncodeOutput;
    Base64Encode(hexresult, &base64EncodeOutput);
    return base64EncodeOutput;
}

int write_header(int new_socket, char *extracted_key, const char *HEADER)
{
    // Send back
    char *head_copy = (char *)malloc(strlen(HEADER) + 200);
    strcpy(head_copy, HEADER);
    strcat(head_copy, "Upgrade: websocket\r\n");
    strcat(head_copy, "Connection: Upgrade\r\n");
    strcat(head_copy, "Sec-WebSocket-Accept: ");
    strcat(head_copy, extracted_key);
    strcat(head_copy, "\r\n\r\n");
    if (send(new_socket, head_copy, strlen(head_copy), 0) < 0)
    {
        return -1;
    };
    free(head_copy);
    return 0;
}

int get_payload_size(const unsigned char *message)
{
    const unsigned int bit_size = (unsigned int)message[1] & 0b01111111;
    int payload_size = 1;
    if (bit_size == 126)
    {
        payload_size = 3;
        // char *ws_message = calloc(1024, sizeof(char));
    }
    else if (bit_size == 127)
    {
        payload_size = 9;
        // Read the next 64 bits
    }
    return payload_size;
}

long get_payload_length(const unsigned char *message)
{
    const unsigned int bit_size = (unsigned int)message[1] & 0b01111111;
    printf("Bit size %i\n", bit_size);
    size_t payload_length = (size_t)bit_size;
    if (bit_size == 126)
    {
        payload_length = (uint16_t)(message[2] << 8) | message[3];
        // char *ws_message = calloc(1024, sizeof(char));
    }
    else if (bit_size == 127)
    {
        // Read the next 64 bits
        uint64_t payload_length =
            (((uint64_t)message[2]) << 56 | ((uint64_t)message[3]) << 48 | ((uint64_t)message[4]) << 40 |
             ((uint64_t)message[5]) << 32 | ((uint64_t)message[6]) << 24 | ((uint64_t)message[7]) << 16 |
             ((uint64_t)message[8]) << 8 | ((uint64_t)message[9]));
    }
    return payload_length;
}

int decode_websocket_message(unsigned char *message, unsigned char **ws_message)
{
    // Decode payload length
    long payload_length = get_payload_length(message);
    // recv again of payload_length from fd;
    printf("Length : %li\n", payload_length);
    int payload_size = get_payload_size(message);
    // 3000 - 1020
    // recv(curr, void *buf, size_t n, int flags)
    *ws_message = (unsigned char *)calloc(payload_length + 1, sizeof(unsigned char));
    const int opcode_size = 1;

    // TOOO : remove this after we use next_bytes function
    int mask_idx = payload_size + opcode_size;
    unsigned char *mask = &message[mask_idx];
    int payload_idx = payload_size + opcode_size + 4;
    unsigned char *payload = &message[payload_idx];
    for (int i = 0; i < payload_length; ++i)
    {
        // TODO : payload[i] is overflow
        // TOOO : use next_bytes function defined
        (*ws_message)[i] = mask[i % 4] ^ payload[i];
    }
    return 0;
}

int send_reply_message(int fd, char *msg)
{
    unsigned char buffer[MAX_PAYLOAD_SIZE] = {0};
    buffer[0] = 0x81;
    // TODO: set appropriate length
    long msg_len = strlen(msg);
    int idx;
    unsigned int code;
    if (msg_len <= 127)
    {
        idx = 2;
        buffer[1] = ((unsigned int)msg_len) & 0b01111111;
    }
    else if (msg_len <= 65535)
    {
        idx = 4;
        buffer[1] = (unsigned int)(126 & 0b01111111);
        buffer[2] = (unsigned char)((msg_len >> 8) & 0xFF);
        buffer[3] = (unsigned char)((msg_len) & 0xFF);
    }
    else
    {
        idx = 10;
        buffer[1] = (unsigned int)(127 & 0b01111111);
        int j = 2;
        for (int i = (64 - 8); i >= 0; i -= 8)
        {
            buffer[j++] = (unsigned char)(msg_len >> i) & 0xFF;
        }
    }
    memcpy(&buffer[idx], msg, strlen(msg));
    if (send(fd, buffer, strlen(msg) + idx, 0) < 0)
    {
        perror("Error in writing");
        return -1;
    };
    return 0;
}

int broadcast_message(int current_fd, int socket_fd, char *ws_message)
{
    for (int i = 0; i < MAX_CLIENT; ++i)
    {
        if (socket_clients[i] == 0 || socket_clients[i]->fd == current_fd || socket_clients[i]->fd == socket_fd)
        {
            continue;
        }
        if (send_reply_message(socket_clients[i]->fd, ws_message) == -1)
        {
            perror("Error when sending to another\n");
            return -1;
        }
    }
    if (send_reply_message(current_fd, ws_message) == -1)
    {
        perror("Error when sending to current\n");
        return -1;
    }

    return 0;
}

enum Opcode
{
    Continuation_Frame = 0x0,
    Text_Frame = 0x1,
    Binary_Frame = 0x2,
    Close_Frame = 0x8,
    Ping_Frame = 0x9,
    Pong_Frame = 0xA,
};

static inline int next_bytes(ws_frame_data *wfd)
{
    size_t n = 0;

    if (wfd->cur_pos == 0 || wfd->cur_pos == wfd->amt_read)
    {
        long n = recv(wfd->fd, wfd->buffer_ws, sizeof(wfd->buffer_ws), 0);
        if (n == -1)
        {
            perror("While reading");
            return -1;
        }
        wfd->amt_read = (size_t)n;
        wfd->cur_pos = 0;
    }
    return (wfd->buffer_ws[wfd->cur_pos++]);
}

int handle_received_message(int current_fd, unsigned char *buffer_ws)
{
    long resp_ws = recv(current_fd, buffer_ws, 1024, 0);
    if (resp_ws < 0)
    {
        perror("Error in reading");
        free(buffer_ws);
        return -1;
    }
    return 0;
}

int handle_text_frame(ws_frame_data *wfd)
{
    unsigned char *ws_message;
    decode_websocket_message(wfd->buffer_ws, &ws_message);
    printf("MESSAGE : %s\n", ws_message);
    // Broadcast
    // broadcast_message(current_fd, socket_fd, (char *)ws_message);
    free(ws_message);
    return 0;
}

int remove_socket_client(ws_frame_data *wfd)
{
    for (int i = 0; i < MAX_CLIENT; ++i)
    {
        if (socket_clients[i] != 0 && socket_clients[i] == wfd)
        {
            socket_clients[i] = 0;
            free(wfd);
            return 0;
        }
    }
    return 0;
}

int handle_close_frame(ws_frame_data *wfd)
{
    if (shutdown(wfd->fd, SHUT_WR) == -1)
    {
        perror("When shutdown");
    };
    char *buf = calloc(1, sizeof(char));
    if (recv(wfd->fd, buf, 1, 0) == 0)
    {
        free(buf);
        remove_socket_client(wfd);
        close(wfd->fd);
        return 0;
    };
    free(buf);
    return 0;
}

void handle_continuation_frame(int current_fd, unsigned char *buffer_ws)
{
    unsigned char *current_payload;
    decode_websocket_message(buffer_ws, &current_payload);
    long sum_payload_length = get_payload_length(buffer_ws);
    while ((buffer_ws[0] & 0xf) == Continuation_Frame)
    {
        // Listen to message

        unsigned char *new_buffer;
        // Concatenate
        unsigned char *new_payload;
        decode_websocket_message(new_buffer, &new_payload);
        long new_payload_length = get_payload_length(new_buffer);
        current_payload = realloc(current_payload, sum_payload_length + new_payload_length);
        // TODO : Fill the empty with new data
        memcpy(current_payload + sum_payload_length, new_payload, new_payload_length * sizeof(unsigned char));
        sum_payload_length += new_payload_length;
        free(new_payload);
        free(buffer_ws);
        // point to new buffer;
        buffer_ws = new_buffer;
    }
}

void handle_continuation_frame_temp(ws_frame_data *wfd)
{
    printf("Continuation_Frame\n");
}

int process_message(ws_frame_data *wfd)
{
    next_bytes(wfd);
    // TODO : currently we just access the buffer_ws
    // Need to update so that we just get value from the returned value from next_bytes function.
    // And then we can update the unmasking process :D
    int opcode = (wfd->buffer_ws[0] & 0xf);
    switch (opcode)
    {
    case Continuation_Frame: {
        handle_continuation_frame_temp(wfd);
        break;
    }
    case Text_Frame: {
        handle_text_frame(wfd);
        break;
    }
    case Close_Frame: {
        handle_close_frame(wfd);
        break;
    }
    default: {
        return -1;
    }
    }
    return 0;
}

// int decode_websocket_message(unsigned char *message, char **ws_message)
// {
//     // Decode payload length
//     // memset(out, '\0', strlen(out));
//     size_t payload_length = get_payload_length(message);
//     int payload_size = get_payload_size(message);
//     int opcode_size = 1;
//
//     *ws_message = (char *)calloc(payload_length + 2, sizeof(char));
//
//     int mask_idx = payload_size + opcode_size;
//     unsigned char *mask = &message[mask_idx];
//     int payload_idx = payload_size + opcode_size + 4;
//     unsigned char *payload = &message[payload_idx];
//     for (int i = 0; i < payload_length; ++i)
//     {
//         (*ws_message)[i] = mask[i % 4] ^ payload[i];
//     }
//     return 0;
// }

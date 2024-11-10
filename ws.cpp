#include "ws.h"
#include <asm-generic/socket.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// const int MAX_PAYLOAD_SIZE = 1024;

char *get_websocket_key(char *buffer, char *copy)
{
    strcpy(copy, buffer);
    char *token = strtok(copy, "\r\n");
    char *websocket_key = nullptr;
    while (token != nullptr)
    {

        if (strstr(token, "Sec-WebSocket-Key:") != nullptr)
        {
            websocket_key = strtok(token, ": ");
            websocket_key = strtok(nullptr, ": ");
        }
        token = strtok(nullptr, "\r\n");
    }
    return websocket_key;
}

char *extract_key(char *websocket_key)
{
    size_t offset;
    char *hexresult = (char *)malloc(41);
    const char *MAGIC_STRING = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    websocket_key = strcat(websocket_key, MAGIC_STRING);
    SHA1((unsigned char *)websocket_key, strlen(websocket_key), (unsigned char *)hexresult);
    printf("Result sha1 : %s\n", hexresult);
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

int decode_websocket_message(char *message, char *out)
{
    // Decode payload length
    memset(out, 0, strlen(out));
    const unsigned int bit_size = (unsigned int)message[1] & 0b01111111;
    const int opcode_size = 1;
    ssize_t payload_length = (ssize_t)bit_size;

    int payload_size = 1;
    if (bit_size == 126)
    {
        payload_size = 3;
        // Read the next 16 bits
        payload_length = (u_int16_t)message[2];
    }
    else if (bit_size == 127)
    {
        payload_size = 9;
        // Read the next 64 bits
        payload_length = (u_int64_t)message[2];
    }
    int mask_idx = payload_size + opcode_size;
    char *mask = &message[mask_idx];
    int payload_idx = payload_size + opcode_size + 4;
    char *payload = &message[payload_idx];
    for (int i = 0; i < payload_length; ++i)
    {
        out[i] = mask[i % 4] ^ payload[i];
    }
    return 0;
}

int send_reply_message(int fd, char *msg)
{
    char buffer[100] = {0};
    buffer[0] = 0x81;
    buffer[1] = (unsigned char)strlen(msg);
    memcpy(&buffer[2], msg, strlen(msg));
    if (send(fd, buffer, strlen(msg) + 2, 0) < 0)
    {
        printf("Error in writing");
        return -1;
    };
    return 0;
}

int handle_received_message(int new_socket, char *buffer_ws)
{
    long resp_ws = recv(new_socket, buffer_ws, 16000, 0);
    if (resp_ws < 0)
    {
        perror("Error in reading");
        return -1;
    }
    return 0;
}

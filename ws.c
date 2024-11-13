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

char *get_websocket_key(char *buffer, char *copy)
{
    strcpy(copy, buffer);
    char *token = strtok(copy, "\r\n");
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

int decode_websocket_message(unsigned char *message, char *out)
{
    // Decode payload length
    memset(out, '\0', strlen(out));
    const unsigned int bit_size = (unsigned int)message[1] & 0b01111111;
    printf("Bit size %i\n", bit_size);
    const int opcode_size = 1;
    size_t payload_length = (size_t)bit_size;

    int payload_size = 1;
    if (bit_size == 126)
    {
        payload_size = 3;
        payload_length = (uint16_t)(message[2] << 8) | message[3];
    }
    else if (bit_size == 127)
    {
        payload_size = 9;
        // Read the next 64 bits
        payload_length = (u_int64_t)message[2];
    }

    int mask_idx = payload_size + opcode_size;
    unsigned char *mask = &message[mask_idx];
    int payload_idx = payload_size + opcode_size + 4;
    unsigned char *payload = &message[payload_idx];
    for (int i = 0; i < payload_length; ++i)
    {
        out[i] = mask[i % 4] ^ payload[i];
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

int handle_received_message(int new_socket, unsigned char *buffer_ws)
{
    long resp_ws = recv(new_socket, buffer_ws, 16000, 0);
    if (resp_ws < 0)
    {
        perror("Error in reading");
        return -1;
    }
    return 0;
}

#ifndef WS_H
#define WS_H
#define MAX_PAYLOAD_SIZE 1024

char *get_websocket_key(char *buffer, char *copy);
char *extract_key(char *websocket_key);
int decode_websocket_message(unsigned char *message, char *out);
int send_reply_message(int fd, char *msg);
int write_header(int new_socket, char *extracted_key, const char *HEADER);
int handle_received_message(int new_socket, unsigned char buffer_ws[]);
int Base64Encode(const char *message, char **buffer);

#endif // !WS_H

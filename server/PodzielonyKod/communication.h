#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <netinet/in.h>

void send_message(int server_socket, char header, const char *message, struct sockaddr_in *dest_addr, socklen_t addr_len);
void send_ack(int server_socket, char received_header, struct sockaddr_in *dest_addr, socklen_t addr_len);
void handle_received_message(int server_socket);

#endif

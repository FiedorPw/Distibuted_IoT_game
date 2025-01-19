#include "communication.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

void send_message(int server_socket, char header, const char *message, struct sockaddr_in *dest_addr, socklen_t addr_len) {
    char buffer[BUFFER_SIZE];
    buffer[0] = header;
    if (message != NULL) {
        strncpy(buffer + 1, message, BUFFER_SIZE - 2);
        buffer[BUFFER_SIZE - 1] = '\0';
    } else {
        buffer[1] = '\0';
    }

    if (dest_addr != NULL) {
        printf("\033[34mWysyłanie %c: %s do %s:%d\033[0m\n", header, message ? message : "NULL", inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));
        sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)dest_addr, addr_len);
    } else {
        printf("\033[31mBłąd: Adres docelowy nie jest ustawiony.\033[0m\n");
    }
}

void send_ack(int server_socket, char received_header, struct sockaddr_in *dest_addr, socklen_t addr_len) {
    char ack_message[2] = {received_header, '\0'};
    send_message(server_socket, ACK, ack_message, dest_addr, addr_len);
}

void handle_received_message(int server_socket) {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    ssize_t recv_len = recvfrom(server_socket, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &client_len);

    if (recv_len < 0) {
        perror("Błąd recvfrom");
        return;
    }

    buffer[recv_len] = '\0';
    char header = buffer[0];
    char *message = buffer + 1;

    printf("\033[35mOtrzymano wiadomość: [%c]%s\033[0m\n", header, message);

    switch (header) {
        case REGISTER:
            handle_registration(server_socket, message, &client_addr, client_len);
            break;
        case ACK:
            printf("\033[32mOtrzymano ACK dla: %s\033[0m\n", message);
            break;
        case WINNER:
            handle_winner_message(server_socket, message, &client_addr, client_len);
            break;
        default:
            printf("Nieznany nagłówek: %c\n", header);
    }
}

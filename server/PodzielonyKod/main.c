#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/select.h>
#include <errno.h>

int server_id;
int num_games;
int current_game = 0;
ClientInfo client = {0};
GameStats *game_stats;
bool game_won = false; // Flaga informująca o wygranej klienta

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Użycie: %s <id_serwera> <liczba_gier>\n", argv[0]);
        return 1;
    }

    server_id = atoi(argv[1]);
    num_games = atoi(argv[2]);
    game_stats = malloc(num_games * sizeof(GameStats));

    printf("Uruchamianie serwera z ID %d, liczba gier: %d\n", server_id, num_games);

    int server_socket;
    struct sockaddr_in server_addr;
    socklen_t client_len = sizeof(client.address);

    // Utworzenie gniazda UDP
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        perror("Błąd tworzenia gniazda");
        return 1;
    }

    // Konfiguracja adresu serwera
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Błąd bind");
        close(server_socket);
        return 1;
    }

    printf("Serwer nasłuchuje na porcie %d\n", SERVER_PORT);

    fd_set readfds;
    struct timeval tv;

    // Zmienna do opóźnienia wysyłania GAME
    unsigned long last_game_message_time = 0;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);

        tv.tv_sec = 0;
        tv.tv_usec = 500000;

        int activity = select(server_socket + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0) {
            perror("Błąd select");
            break;
        }

        if (FD_ISSET(server_socket, &readfds)) {
            char buffer[BUFFER_SIZE];
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            ssize_t recv_len = recvfrom(server_socket, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &client_len);

            if (recv_len < 0) {
                perror("Błąd recvfrom");
                continue;
            }

            buffer[recv_len] = '\0';
            char header = buffer[0];
            char *message = buffer + 1;

            printf("\033[35mOtrzymano wiadomość: [%c]%s\033[0m\n", header, message);

            switch (header) {
                case REGISTER:
                    if (!client.registered) {
                        handle_registration(server_socket, message, &client_addr, client_len);
                        if (client.registered && current_game < num_games) {
                            printf("\033[32mRozpoczynanie gry dla klienta ID: %d\033[0m\n", client.id);
                            client.game_started = true;
                            game_stats[current_game].throws = 0;
                            game_won = false;
                            send_message(server_socket, START, NULL, &client.address, client_len);
                        }
                    } else {
                        printf("Klient już zarejestrowany.\n");
                        send_ack(server_socket, REGISTER, &client_addr, client_len);
                    }
                    break;
                case ACK:
                    printf("\033[32mOtrzymano ACK dla: %s\033[0m\n", message);
                    if (strlen(message) > 1) {
                        message++;
                        if (message[0] == WINNER) {
                            printf("Odebrano WINNER po ACK\n");
                            handle_winner_message(server_socket, message + 1, &client_addr, client_len);
                        }
                    }
                    break;
                case WINNER:
                    printf("Odebrano komunikat: WINNER\n");
                    handle_winner_message(server_socket, message, &client_addr, client_len);
                    break;
                default:
                    printf("Nieznany nagłówek: %c\n", header);
            }
        } else {
            if (client.game_started && client.registered && !game_won) {
                if (time(NULL) - last_game_message_time >= 1) {
                    send_game_message(server_socket, &client.address, client_len);
                    last_game_message_time = time(NULL);
                }
            }
        }

        if (game_won) {
            if (client.registered && current_game < num_games) {
                printf("\033[32mRozpoczynanie nowej gry dla klienta ID: %d\033[0m\n", client.id);
                client.game_started = true;
                game_stats[current_game].throws = 0;
                game_won = false;
                send_message(server_socket, START, NULL, &client.address, client_len);
            }
            game_won = false;
        }
    }

    close(server_socket);
    free(game_stats);
    return 0;
}

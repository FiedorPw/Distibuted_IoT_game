// registration.c
#include "server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

void handle_registration(int server_socket, char *message, struct sockaddr_in *client_addr, socklen_t client_len) {
    printf("\033[32mOdebrano żądanie rejestracji\033[0m\n");

    if (game.registered_players >= game.num_players) {
        printf("Gra już pełna.\n");
        return;
    }

    int free_index = find_free_player_index();
    if (free_index == -1) {
        printf("Brak wolnych miejsc dla nowych klientów.\n");
        return;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr->sin_port);
    int client_id;

    if (sscanf(message, "%[^,],%d,%d", client_ip, &client_port, &client_id) == 3) {
        game.players[free_index].address = *client_addr;
        game.players[free_index].id = client_id;
        game.players[free_index].registered = true;
        game.players[free_index].wins = 0;
        game.registered_players++;
        printf("Zarejestrowano klienta ID: %d, IP: %s, Port: %d\n", client_id, client_ip, client_port);
        send_ack(server_socket, REGISTER, client_addr, client_len);

        if (game.registered_players == game.num_players) {
            printf("\033[32mWszyscy gracze zarejestrowani. Rozpoczynanie gry.\033[0m\n");
            game.game_started = true;
            game.throws = 0;
            strcpy(game.result, "");
            game.last_game_message_time = 0;
            game.current_game_num = 1;

            for (int i = 0; i < game.num_players; i++) {
                send_message(server_socket, START, NULL, &game.players[i].address, sizeof(game.players[i].address));
            }
        }
    } else {
        printf("Błąd podczas parsowania danych rejestracyjnych.\n");
    }
}

void handle_winner_message(int server_socket, char *message, struct sockaddr_in *client_addr, socklen_t client_len) {
    int client_index = find_player_index(client_addr);
    if (client_index == -1) {
        printf("Nieznany klient wysłał wiadomość WINNER.\n");
        return;
    }

    printf("\033[32mOdebrano komunikat WINNER od klienta ID %d: %s\033[0m\n", game.players[client_index].id, message);

    if (game.game_started) {
        if (message[0] == '1') {
            printf("Ktoś wygrał!\n");
            strcpy(game.result, "W");
            game.game_started = false;
            game.players[client_index].wins++;
            strcpy(game.winning_sequence, game.current_sequence);
            game.winning_throws_per_game[game.current_game_num - 1] = game.throws;
        } else if (message[0] == '0') {
            printf("Ktoś przegrał!\n");
            strcpy(game.result, "P");
            game.game_started = false;
            strcpy(game.winning_sequence, game.current_sequence);
            game.winning_throws_per_game[game.current_game_num - 1] = game.throws;
        }

        send_winner_notification(server_socket);

        if (game.current_game_num < game.total_games) {
            game.current_game_num++;
            printf("\033[32mRozpoczynanie gry %d\033[0m\n", game.current_game_num);
            game.game_started = true;
            game.throws = 0;
            strcpy(game.result, "");
            strcpy(game.current_sequence, "");
            game.last_game_message_time = 0;

            for (int i = 0; i < game.num_players; i++) {
                send_message(server_socket, START, NULL, &game.players[i].address, sizeof(game.players[i].address));
            }
        } else {
            printf("\033[32mWszystkie gry zostały rozegrane.\033[0m\n");
            for (int i = 0; i < game.num_players; i++) {
                printf("Gracz %d wygrał %d razy.\n", game.players[i].id, game.players[i].wins);
                if (game.total_games > 0) {
                    double win_ratio = (double)game.players[i].wins / game.total_games;
                    printf("Szansa na wygraną gracza %d: %.2f%%\n", game.players[i].id, win_ratio * 100);
                }
            }

            printf("\nStatystyki gier:\n");
            for (int i = 1; i <= game.total_games; i++) {
                printf("Gra %d: Liczba rzutów: %d", i, game.winning_throws_per_game[i-1]);

                int start_index = 0;
                int sequence_length = 0;

                char temp_winning_sequence[MAX_SEQUENCE_LENGTH];
                strcpy(temp_winning_sequence, game.winning_sequence);

                char *token = strtok(temp_winning_sequence, ",");
                while (token != NULL) {
                    sequence_length++;
                    token = strtok(NULL, ",");
                }

                if (sequence_length > SEQUENCE_LENGTH) {
                    start_index = sequence_length - SEQUENCE_LENGTH;
                }

                strcpy(temp_winning_sequence, game.winning_sequence);
                
                token = strtok(temp_winning_sequence, ",");
                for (int j = 0; j < start_index; j++) {
                    token = strtok(NULL, ",");
                }

                printf(", Zwycięska sekwencja: ");
                for (int j = 0; j < SEQUENCE_LENGTH; j++) {
                    if (token != NULL) {
                        printf("%s", token);
                        token = strtok(NULL, ",");
                        if (j < SEQUENCE_LENGTH - 1 && token != NULL)
                            printf(",");
                    }
                }
                printf("\n");
            }
        }
    } else {
        send_ack(server_socket, WINNER, client_addr, client_len);
    }
}

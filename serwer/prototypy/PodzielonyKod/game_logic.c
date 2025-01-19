// game_logic.c
#include "server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <unistd.h>

// Zmienne globalne
int seeded = 0;
int server_id;
GameInfo game;

// Nowe zmienne globalne
int total_number_of_games = 0;
int total_number_of_tosses = 0;
int total_number_of_players = 0;
double total_probability_of_winning = 0.0;
double total_average_tosses = 0.0;

// Funkcja generująca losową liczbę 0 lub 1
int get_random_number() {
    if (!seeded) {
        srand(time(NULL) + server_id);
        seeded = 1;
    }
    return rand() % 2;
}

// Funkcja do znalezienia indeksu klienta w tablicy players
int find_player_index(struct sockaddr_in *client_addr) {
    for (int i = 0; i < game.num_players; i++) {
        if (game.players[i].registered &&
            game.players[i].address.sin_addr.s_addr == client_addr->sin_addr.s_addr &&
            game.players[i].address.sin_port == client_addr->sin_port) {
            return i;
        }
    }
    return -1;
}

// Funkcja do znalezienia wolnego indeksu dla nowego klienta
int find_free_player_index() {
    for (int i = 0; i < game.num_players; i++) {
        if (!game.players[i].registered) {
            return i;
        }
    }
    return -1;
}

// Funkcja wysyłająca wiadomość
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

void send_winner_notification(int server_socket) {
    for (int i = 0; i < game.num_players; i++) {
        if (game.players[i].registered) {
            send_message(server_socket, WINNER, "W0", &game.players[i].address, sizeof(game.players[i].address));
        }
    }
    printf("Wysłano W0 (koniec gry) do wszystkich graczy.\n");
}

void send_game_message_to_all(int server_socket) {
    char server_num[2];
    server_num[0] = get_random_number() + '0';
    server_num[1] = '\0';

    if (game.throws > 0) {
        strcat(game.current_sequence, ",");
    }
    strcat(game.current_sequence, server_num);

    for (int i = 0; i < game.num_players; i++) {
        if (game.players[i].registered) {
            send_message(server_socket, GAME, server_num, &game.players[i].address, sizeof(game.players[i].address));
        }
    }

    printf("Wysłano %s do wszystkich graczy. game.throws: %d, Sekwencja: %s\n", server_num, game.throws, game.current_sequence);
    game.throws++;
    game.last_game_message_time = time(NULL);
}

// Nowa funkcja do podsumowania statystyk
void summarize_statistics() {
    printf("\n\033[32mStatystyki globalne:\033[0m\n");
    printf("Liczba graczy: %d\n", game.num_players);
    printf("Długość wzorców: %d\n", SEQUENCE_LENGTH);
    printf("Liczba rozegranych gier: %d\n", total_number_of_games);
    printf("Szacunkowe prawdopodobieństwo wygranej: %.2f%%\n", total_probability_of_winning / total_number_of_games * 100);
    printf("Średnia liczba rzutów: %.2f\n", total_average_tosses / total_number_of_games);
}

void update_statistics() {
    // Liczymy sumy do późniejszego obliczenia średnich
    total_number_of_players = game.num_players;

    // Obliczanie prawdopodobieństwa wygranej i średniej liczby rzutów
    for (int i = 0; i < game.num_players; i++) {
        total_probability_of_winning += (double)game.players[i].wins / game.total_games;
    }

    total_number_of_games++;
    total_average_tosses += game.throws;
}

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
            summarize_statistics();
        }

        update_statistics();
    } else {
        send_ack(server_socket, WINNER, client_addr, client_len);
    }
}

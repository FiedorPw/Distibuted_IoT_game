#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

GameInfo game;

void init_game() {
    game.players = malloc(game.num_players * sizeof(ClientInfo));
    if (game.players == NULL) {
        perror("Błąd alokacji pamięci dla game.players");
        exit(1);
    }
    game.winning_throws_per_game = malloc(game.total_games * sizeof(int));
    if (game.winning_throws_per_game == NULL) {
        perror("Błąd alokacji pamięci dla game.winning_throws_per_game");
        exit(1);
    }

    for (int i = 0; i < game.num_players; i++) {
        game.players[i].registered = false;
        game.players[i].wins = 0;
    }
    game.registered_players = 0;
    game.game_started = false;
    memset(game.current_sequence, 0, sizeof(game.current_sequence));
    memset(game.winning_sequence, 0, sizeof(game.winning_sequence));
}

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

int find_free_player_index() {
    for (int i = 0; i < game.num_players; i++) {
        if (!game.players[i].registered) {
            return i;
        }
    }
    return -1;
}

void handle_game_logic(int server_socket) {
    if (game.game_started && game.throws < MAX_THROWS && strlen(game.result) == 0) {
        if (time(NULL) - game.last_game_message_time >= 1) {
            send_game_message_to_all(server_socket);
        }
    }
}

void send_winner_notification(int server_socket) {
    for (int i = 0; i < game.num_players; i++) {
        if (game.players[i].registered) {
            send_message(server_socket, WINNER, "W0", &game.players[i].address, sizeof(game.players[i].address));
        }
    }
    printf("Wysłano W0 (koniec gry) do wszystkich graczy.\n");
}

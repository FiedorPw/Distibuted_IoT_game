#include "server.h"
#include "game.h"
#include "communication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

int server_id;

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Użycie: %s <id_serwera> <liczba_graczy> <liczba_gier>\n", argv[0]);
        return 1;
    }

    server_id = atoi(argv[1]);
    game.num_players = atoi(argv[2]);
    game.total_games = atoi(argv[3]);

    init_game();

    int server_socket;
    struct sockaddr_in server_addr;

    server_socket = create_server_socket();
    if (server_socket < 0) {
        return 1;
    }

    if (bind_server_socket(server_socket, &server_addr) < 0) {
        close(server_socket);
        return 1;
    }

    printf("Serwer nasłuchuje na porcie %d\n", SERVER_PORT);

    fd_set readfds;
    struct timeval tv;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        int max_sd = server_socket;

        tv.tv_sec = 0;
        tv.tv_usec = 500000;

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0) {
            perror("Błąd select");
            break;
        }

        if (FD_ISSET(server_socket, &readfds)) {
            handle_received_message(server_socket);
        } else {
            handle_game_logic(server_socket);
        }

        if (!game.game_started && game.current_game_num > game.total_games) {
            printf("Rozegrano wszystkie gry. Zamykanie serwera.\n");
            break;
        }
    }

    free(game.players);
    free(game.winning_throws_per_game);
    close(server_socket);
    return 0;
}

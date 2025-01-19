#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <netinet/in.h>

typedef struct {
    struct sockaddr_in address;
    int id;
    bool registered;
    int wins;
} ClientInfo;

typedef struct {
    bool game_started;
    int num_players;
    int registered_players;
    int throws;
    int current_game_num;
    int total_games;
    char result[2];
    unsigned long last_game_message_time;
    ClientInfo *players;
    char current_sequence[MAX_SEQUENCE_LENGTH];
    char winning_sequence[MAX_SEQUENCE_LENGTH];
    int *winning_throws_per_game;
} GameInfo;

extern GameInfo game;
extern int server_id;

void init_game();
int find_player_index(struct sockaddr_in *client_addr);
int find_free_player_index();
void send_game_message_to_all(int server_socket);
void send_winner_notification(int server_socket);

#endif

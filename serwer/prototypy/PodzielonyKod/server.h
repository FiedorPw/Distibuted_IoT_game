// server.h
#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include <stdbool.h>

#define SERVER_PORT 1305
#define BUFFER_SIZE 255
#define MAX_THROWS 100
#define MAX_SEQUENCE_LENGTH (MAX_THROWS * 2)
#define SEQUENCE_LENGTH 4

#define START 'S'
#define WINNER 'W'
#define REGISTER 'R'
#define GAME 'G'
#define ACK 'a'

// Struktury
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

// Deklaracje funkcji
int get_random_number();
int find_player_index(struct sockaddr_in *client_addr);
int find_free_player_index();
void send_message(int server_socket, char header, const char *message, struct sockaddr_in *dest_addr, socklen_t addr_len);
void send_ack(int server_socket, char received_header, struct sockaddr_in *dest_addr, socklen_t addr_len);
void send_winner_notification(int server_socket);
void send_game_message_to_all(int server_socket);
void handle_registration(int server_socket, char *message, struct sockaddr_in *client_addr, socklen_t client_len);
void handle_winner_message(int server_socket, char *message, struct sockaddr_in *client_addr, socklen_t client_len);

extern GameInfo game;
extern int server_id;
extern int seeded;

// Nowe zmienne globalne do podsumowań
extern int total_number_of_games;
extern int total_number_of_tosses;
extern int total_number_of_players;
extern double total_probability_of_winning;
extern double total_average_tosses;

#endif // SERVER_H

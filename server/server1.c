#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>     // Biblioteka zawierająca funkcje do obsługi socketów
#include <arpa/inet.h>      // Biblioteka dla operacji internetowych
#include <unistd.h>         // Dla funkcji close() i sleep()
#include <time.h>           // Dla funkcji time() - obsługa czasu
#include <stdint.h>         // Dla pracy z liczbami całkowitymi 8-bitowymi

#define ORZEL 1 // Orzeł lata wysoko, więc stan wysoki XD
#define RESZKA 0
#define GAME_LEN 4 // Zmieniamy na 4, bo klient używa 4-bitowych sekwencji

// Deklaracje funkcji
void send_ack(int sock, struct sockaddr_in *client_addr, socklen_t addr_len, char header);
int find_sequence_position(const char *sequence, const char *subsequence);
void process_packet(int sock, struct sockaddr_in *client_addr, socklen_t addr_len, char *buffer);
void start_game(int n);
void print_game_statistics();
void generate_server_sequence();
void handle_register(int sock, struct sockaddr_in *client_addr, socklen_t addr_len);
void handle_game(int sock, struct sockaddr_in *client_addr, socklen_t addr_len, const char *message);
void send_start(int sock, struct sockaddr_in *client_addr, socklen_t addr_len);
void handle_end_game(int sock, struct sockaddr_in *client_addr, socklen_t addr_len);

// Zmienna globalna
int game_active = 0;  // Określa, czy gra jest aktywna
float a_wins = 0, b_wins = 0;
int games_played = 0;
int total_tosses = 0;
int game_length = 4;  // Długość sekwencji gry - 4 bitów
char server_sequence[GAME_LEN + 1];  // Sekwencja serwera

// Funkcja wysyłająca ACK
void send_ack(int sock, struct sockaddr_in *client_addr, socklen_t addr_len, char header) {
    char ack[3] = {header, '\0'};
    sendto(sock, ack, strlen(ack), 0, (struct sockaddr *)client_addr, addr_len);
    printf("ACK sent for %c.\n", header);
}

// Funkcja do wysyłania komunikatu startowego 'S'
void send_start(int sock, struct sockaddr_in *client_addr, socklen_t addr_len) {
    char start_msg[2] = {'S', '\0'};  // Nagłówek 'S' dla rozpoczęcia gry
    sendto(sock, start_msg, strlen(start_msg), 0, (struct sockaddr *)client_addr, addr_len);
    printf("Start message sent (S).\n");
}

// Funkcja do zakończenia gry
void handle_end_game(int sock, struct sockaddr_in *client_addr, socklen_t addr_len) {
    game_active = 0;
    send_ack(sock, client_addr, addr_len, 'G');  // Potwierdzenie zakończenia gry
    printf("Game ended.\n");
}

// Funkcja do generowania sekwencji serwera
void generate_server_sequence() {
    for (int i = 0; i < GAME_LEN; i++) {
        server_sequence[i] = (rand() % 2) ? '1' : '0';
    }
    server_sequence[GAME_LEN] = '\0';
    printf("Generated server sequence: %s\n", server_sequence);
}

// Funkcja do obsługi rejestracji
void handle_register(int sock, struct sockaddr_in *client_addr, socklen_t addr_len) {
    printf("Handling REGISTER request.\n");

    send_ack(sock, client_addr, addr_len, 'R');
    
    // Po rejestracji wysyłamy komunikat startowy
    send_start(sock, client_addr, addr_len);
    
    generate_server_sequence();
    game_active = 1;
    printf("Game activated. Waiting for client sequences.\n");
}

// Funkcja do znalezienia pozycji subsekwencji w sekwencji
int find_sequence_position(const char *sequence, const char *subsequence) {
    char *pos = strstr(sequence, subsequence);
    return pos ? (int)(pos - sequence) : -1;
}

// Funkcja do obsługi gry
void handle_game(int sock, struct sockaddr_in *client_addr, socklen_t addr_len, const char *message) {
    printf("Handling GAME request. Received: %s\n", message);

    if (strlen(message) < GAME_LEN * 2) {
        printf("Invalid input length. Expected two sequences of %d bits each.\n", GAME_LEN);
        return;
    }

    char sequence1[GAME_LEN + 1];
    char sequence2[GAME_LEN + 1];

    strncpy(sequence1, message, GAME_LEN);
    sequence1[GAME_LEN] = '\0';

    strncpy(sequence2, message + GAME_LEN, GAME_LEN);
    sequence2[GAME_LEN] = '\0';

    printf("Received sequences: %s and %s\n", sequence1, sequence2);

    int pos1 = find_sequence_position(server_sequence, sequence1);
    int pos2 = find_sequence_position(server_sequence, sequence2);

    if (pos1 == -1 && pos2 == -1) {
        printf("Neither sequence found. Game continues.\n");
    } else if (pos1 != -1 && (pos2 == -1 || pos1 < pos2)) {
        printf("Sequence %s appears first. Client wins with %s.\n", sequence1, sequence1);
        char winner_msg[GAME_LEN + 2] = {'W', '\0'};
        strncpy(winner_msg + 1, sequence1, GAME_LEN);
        winner_msg[GAME_LEN + 1] = '\0';
        sendto(sock, winner_msg, strlen(winner_msg), 0, (struct sockaddr *)client_addr, addr_len);
        a_wins++;
        games_played++;
        total_tosses += GAME_LEN;
        handle_end_game(sock, client_addr, addr_len);
    } else {
        printf("Sequence %s appears first. Client wins with %s.\n", sequence2, sequence2);
        char winner_msg[GAME_LEN + 2] = {'W', '\0'};
        strncpy(winner_msg + 1, sequence2, GAME_LEN);
        winner_msg[GAME_LEN + 1] = '\0';
        sendto(sock, winner_msg, strlen(winner_msg), 0, (struct sockaddr *)client_addr, addr_len);
        b_wins++;
        games_played++;
        total_tosses += GAME_LEN;
        handle_end_game(sock, client_addr, addr_len);
    }
}

// Funkcja do przetwarzania pakietów
void process_packet(int sock, struct sockaddr_in *client_addr, socklen_t addr_len, char *buffer) {
    char header = buffer[0];
    char *message = buffer + 1;

    switch (header) {
        case 'R':
            handle_register(sock, client_addr, addr_len);
            break;
        case 'G':
            if (game_active) {
                handle_game(sock, client_addr, addr_len, message);
            } else {
                printf("No active game. Ignoring GAME request.\n");
            }
            break;
        case 'W':
            printf("Received WINNER message\n");
            break;
        case 'S':
            printf("Received START message (S)\n");
            break;
        default:
            printf("Unknown packet type: %c\n", header);
    }
}

// Funkcja do rozpoczęcia gry
void start_game(int n) {
    printf("Starting %d games...\n", n);
    for (int i = 0; i < n; i++) {
        printf("Game %d started!\n", i+1);

        uint8_t a_sequence = rand() % (1 << 3);  // Generowanie losowej sekwencji 3-bitowej
        uint8_t b_sequence = rand() % (1 << 3);  // Generowanie losowej sekwencji 3-bitowej
        uint8_t game_sequence = rand() % (1 << game_length);  // Generowanie losowej sekwencji gry

        printf("Game sequence (bitwise): %d\n", game_sequence);
        printf("Player A sequence (bitwise): %d\n", a_sequence);
        printf("Player B sequence (bitwise): %d\n", b_sequence);

        char winner = 'R'; // Draw
        int a_count = 0, b_count = 0;

        for (int i = 0; i < game_length; i++) {
            int current_nth_bit = (game_sequence >> i) & 1;
            int current_a_bit = (a_sequence >> a_count) & 1;
            int current_b_bit = (b_sequence >> b_count) & 1;

            if (current_a_bit == current_nth_bit) {
                a_count++;
            } else {
                a_count = 0;
            }

            if (current_b_bit == current_nth_bit) {
                b_count++;
            } else {
                b_count = 0;
            }

            if (a_count == 3) {
                winner = 'A';
                break;
            }

            if (b_count == 3) {
                winner = 'B';
                break;
            }
        }

        if (winner == 'A') {
            a_wins++;
        } else if (winner == 'B') {
            b_wins++;
        }

        games_played++;
        total_tosses += game_length;
        printf("Winner: Player %c\n", winner);
    }
}

// Funkcja do drukowania statystyk gry
void print_game_statistics() {
    printf("\nGame Statistics:\n");
    printf("Games played: %d\n", games_played);
    printf("A wins: %.2f\n", a_wins);
    printf("B wins: %.2f\n", b_wins);
    printf("A win probability: %.2f%%\n", (a_wins / games_played) * 100);
    printf("B win probability: %.2f%%\n", (b_wins / games_played) * 100);
    printf("Total tosses: %d\n", total_tosses);
    printf("Average tosses per game: %.2f\n", (float)total_tosses / games_played);
}

int main() {
    srand(time(NULL));

    // Setup serwera UDP
    int sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[1024];

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(1305);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Server is running and listening on port 1305.\n");

    // Główna pętla odbierająca i przetwarzająca wiadomości
    while (1) {
        ssize_t len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (len < 0) {
            perror("Error receiving data");
            continue;
        }

        buffer[len] = '\0';
        process_packet(sock, &client_addr, addr_len, buffer);
    }

    close(sock);
    return 0;
}

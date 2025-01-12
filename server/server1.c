#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>     // Biblioteka zawierająca funkcje do obsługi socketów
#include <arpa/inet.h>      // Biblioteka dla operacji internetowych
#include <unistd.h>         // Dla funkcji close() i sleep()
#include <time.h>           // Dla funkcji time() - obsługa czasu
#include <sys/time.h>       // Dla funkcji gettimeofday() - precyzyjny pomiar czasu
#include <stdint.h>

#define ORZEL 1 //orzeł lata wysoko więc stan wysoki XD
#define RESZKA 0
#define GAME_LEN 8

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 256
#define PATTERN_LENGTH 3
#define GAME_SEQUENCE_LENGTH 8
#define ALP_PORT 12345

// Server state
static int num_clients = 0;
static struct sockaddr_in client_addresses[MAX_CLIENTS];
static int tosses_needed[MAX_CLIENTS];
static uint8_t client_patterns[MAX_CLIENTS];

int seeded = 0;

// Generate random bit sequence
uint8_t generate_bit_sequence(int length) {
    uint8_t sequence = 0;
    for (int i = 0; i < length; i++) {
        sequence = (sequence << 1) | (rand() % 2);
    }
    return sequence;
}

// Send ALP message to a client
void send_message(int sockfd, struct sockaddr_in *client, uint8_t *message, size_t length) {
    sendto(sockfd, message, length, 0, (struct sockaddr *)client, sizeof(*client));
}

// Process client registration
void handle_register(int sockfd, struct sockaddr_in *client, uint8_t *buffer) {
    if (num_clients < MAX_CLIENTS) {
        client_patterns[num_clients] = buffer[1]; // Assume pattern is in the second byte
        client_addresses[num_clients] = *client;
        tosses_needed[num_clients] = -1; // Initialize toss count
        num_clients++;
        printf("Client registered. Total clients: %d\n", num_clients);
    } else {
        printf("Client registration failed: max clients reached.\n");
    }
}

// Broadcast a message to all clients
void broadcast_message(int sockfd, uint8_t *message, size_t length) {
    for (int i = 0; i < num_clients; i++) {
        send_message(sockfd, &client_addresses[i], message, length);
    }
}

// Main game logic
void play_game(int sockfd) {
    uint8_t game_sequence = generate_bit_sequence(GAME_SEQUENCE_LENGTH);
    printf("Game sequence: %u\n", game_sequence);

    uint8_t start_message[2] = {0x01, game_sequence}; // Start game message
    broadcast_message(sockfd, start_message, sizeof(start_message));

    int winner = -1;
    int toss_count = 0;

    while (winner == -1) {
        uint8_t toss = generate_bit_sequence(1); // Generate single toss
        uint8_t toss_message[2] = {0x02, toss}; // Toss message

        broadcast_message(sockfd, toss_message, sizeof(toss_message));
        toss_count++;

        // Wait for reports (simplified, no retries)
        for (int i = 0; i < num_clients; i++) {
            uint8_t buffer[BUFFER_SIZE];
            struct sockaddr_in client;
            socklen_t client_len = sizeof(client);
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr *)&client, &client_len);

            if (n > 0 && buffer[0] == 0x03) { // Report message
                winner = buffer[1];
                printf("Winner: Client %d after %d tosses\n", winner, toss_count);
                break;
            }
        }
    }

    // Record results
    printf("Game complete. Winner: Client %d\n", winner);
}

int main() {
    srand(time(NULL));
    seeded = 1;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(ALP_PORT);

    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", ALP_PORT);

    uint8_t buffer[BUFFER_SIZE];
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    while (1) {
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client, &client_len);
        if (n > 0) {
            switch (buffer[0]) {
                case 0x00: // Register message
                    handle_register(sockfd, &client, buffer);
                    break;
                case 0x01: // Start game command (from admin, e.g.)
                    play_game(sockfd);
                    break;
                default:
                    printf("Unknown message type: %u\n", buffer[0]);
            }
        }
    }

    close(sockfd);
    return 0;
}


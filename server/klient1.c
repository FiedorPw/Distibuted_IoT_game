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
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_PORT 12345
#define BUFFER_SIZE 256
#define PATTERN_LENGTH 3

// Client state
static uint8_t pattern;
static uint8_t tossing_history = 0;
static int toss_count = 0;

// Send ALP message to server
void send_message(int sockfd, struct sockaddr_in *server, uint8_t *message, size_t length) {
    sendto(sockfd, message, length, 0, (struct sockaddr *)server, sizeof(*server));
}

// Check if the pattern matches the tossing history
int check_pattern() {
    uint8_t mask = (1 << PATTERN_LENGTH) - 1; // Mask for last PATTERN_LENGTH bits
    return (tossing_history & mask) == pattern;
}

int main() {
    // Configure server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Localhost

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Input pattern
    printf("Enter a %d-bit pattern (as binary): ", PATTERN_LENGTH);
    char pattern_input[PATTERN_LENGTH + 1];
    scanf("%s", pattern_input);

    // Convert binary string to uint8_t
    pattern = 0;
    for (int i = 0; i < PATTERN_LENGTH; i++) {
        pattern = (pattern << 1) | (pattern_input[i] - '0');
    }

    // Register with the server
    uint8_t register_message[2] = {0x00, pattern};
    send_message(sockfd, &server_addr, register_message, sizeof(register_message));

    printf("Registered with server. Waiting for game to start...\n");

    uint8_t buffer[BUFFER_SIZE];
    while (1) {
        struct sockaddr_in sender;
        socklen_t sender_len = sizeof(sender);

        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&sender, &sender_len);
        if (n > 0) {
            switch (buffer[0]) {
                case 0x01: // Start game message
                    tossing_history = 0;
                    toss_count = 0;
                    printf("Game started.\n");
                    break;

                case 0x02: // Toss message
                    toss_count++;
                    uint8_t toss = buffer[1];
                    tossing_history = (tossing_history << 1) | toss;

                    if (check_pattern()) {
                        printf("Pattern matched after %d tosses. Reporting to server.\n", toss_count);
                        uint8_t report_message[2] = {0x03, 0x01}; // Report message with client ID 1 (for simplicity)
                        send_message(sockfd, &server_addr, report_message, sizeof(report_message));
                    }
                    break;

                default:
                    printf("Unknown message type: %u\n", buffer[0]);
            }
        }
    }

    close(sockfd);
    return 0;
}

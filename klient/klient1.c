#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#define SERVER_PORT 12345
#define BUFFER_SIZE 256
#define PATTERN_LENGTH 3

// Client state
static uint8_t pattern_A;
static uint8_t pattern_B;
static uint8_t tossing_history = 0;
static int toss_count = 0;

// Send ALP message to server
void send_message(int sockfd, struct sockaddr_in *server, uint8_t *message, size_t length) {
    sendto(sockfd, message, length, 0, (struct sockaddr *)server, sizeof(*server));
}

// Check if the pattern matches the tossing history
int check_pattern(uint8_t pattern) {
    uint8_t mask = (1 << PATTERN_LENGTH) - 1; // Mask for last PATTERN_LENGTH bits
    return (tossing_history & mask) == pattern;
}

// Simulate button press for ORZEŁ (1) or RESZKA (0)
uint8_t get_user_input() {
    char choice;
    printf("Enter '1' for ORZEŁ, '0' for RESZKA: ");
    scanf(" %c", &choice);
    return (choice == '1') ? 1 : 0;
}

// Simulate GPIO LED feedback for win or lose
void simulate_gpio_feedback(int won) {
    if (won) {
        printf("LED ON FAST! (You won!)\n");
    } else {
        printf("LED ON SLOW! (You lost!)\n");
    }
}

int main() {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    printf("Client A, Enter a %d-bit pattern (as binary): ", PATTERN_LENGTH);
    char pattern_input_A[PATTERN_LENGTH + 1];
    scanf("%s", pattern_input_A);
    pattern_A = 0;
    for (int i = 0; i < PATTERN_LENGTH; i++) {
        pattern_A = (pattern_A << 1) | (pattern_input_A[i] - '0');
    }

    printf("Client B, Enter a %d-bit pattern (as binary): ", PATTERN_LENGTH);
    char pattern_input_B[PATTERN_LENGTH + 1];
    scanf("%s", pattern_input_B);
    pattern_B = 0;
    for (int i = 0; i < PATTERN_LENGTH; i++) {
        pattern_B = (pattern_B << 1) | (pattern_input_B[i] - '0');
    }

    uint8_t register_message_A[2] = {0x00, pattern_A};
    send_message(sockfd, &server_addr, register_message_A, sizeof(register_message_A));
    uint8_t register_message_B[2] = {0x00, pattern_B};
    send_message(sockfd, &server_addr, register_message_B, sizeof(register_message_B));

    printf("Registered with server. Waiting for game to start...\n");

    uint8_t buffer[BUFFER_SIZE];
    while (1) {
        struct sockaddr_in sender;
        socklen_t sender_len = sizeof(sender);
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&sender, &sender_len);
        if (n > 0) {
            switch (buffer[0]) {
                case 0x01: // Start game
                    tossing_history = 0;
                    toss_count = 0;
                    printf("Game started.\n");
                    break;

                case 0x02: // Toss
                    toss_count++;
                    uint8_t toss = buffer[1];
                    tossing_history = (tossing_history << 1) | toss;

                    if (check_pattern(pattern_A)) {
                        printf("Client A's pattern matched after %d tosses.\n", toss_count);
                        uint8_t report_message[2] = {0x03, 0x01};
                        send_message(sockfd, &server_addr, report_message, sizeof(report_message));
                        simulate_gpio_feedback(1);  // Fast LED for Client A
                        return 0;  // End when A wins
                    } else if (check_pattern(pattern_B)) {
                        printf("Client B's pattern matched after %d tosses.\n", toss_count);
                        uint8_t report_message[2] = {0x03, 0x02};
                        send_message(sockfd, &server_addr, report_message, sizeof(report_message));
                        simulate_gpio_feedback(0);  // Slow LED for Client B
                        return 0;  // End when B wins
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

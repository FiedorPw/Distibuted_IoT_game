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
#include <time.h>

static int seeded = 0;
static int games_played = 0;
static int total_tosses = 0;
static int total_players = 2; // We assume 2 players, adjust as needed
static int game_length = 8; // Length of game sequence (could vary)
static float a_wins = 0, b_wins = 0;

// tak jakby array[n] tylko że dla bitów i na odwrót(bierze od końca)
int get_nth_bit(uint8_t sequence, int n){
    return (sequence >> n) & 1;
}

// Generating random game sequence
uint8_t generate_bit_sequence(int lenght){
    uint8_t sequence = 0;
    for (int i = 0; i < lenght; i++){
        sequence = sequence << 1;
        sequence = sequence | (rand() % 2);
    }
    return sequence;
}

void print_bit_sequence(uint8_t sequence, int lenght){
    printf("Game sequence (bitwise): ");
    for (int i = lenght-1; i >= 0; i--) {
        printf("%d", (sequence >> i) & 1);
    }
    printf("\n");
}

char get_winner(uint8_t a_sequence, uint8_t b_sequence, uint8_t game_sequence){
    char winner = 'R'; // Draw
    int a_count = 0;
    int b_count = 0;

    for (int i = 0; i <= 7; i++){
        int current_nth_bit = get_nth_bit(game_sequence, i);
        int current_a_bit = get_nth_bit(a_sequence, a_count);
        int current_b_bit = get_nth_bit(b_sequence, b_count);

        if(current_a_bit == current_nth_bit){
            a_count++;
        }else{
            a_count = 0;
        }

        if(current_b_bit == current_nth_bit){
            b_count++;
        }else{
            b_count = 0;
        }
        if (a_count == 3){
            return 'A';
        }

        if(b_count == 3){
            return 'B';
        }
    }

    return winner;
}

void gpio_feedback(int winner) {
    // Simulating GPIO actions for feedback
    if (winner == 1) {
        printf("LED ON FAST! (You won!)\n");
        // Here, you would interact with GPIO to turn on the LED fast (winner)
        // Example: gpioWrite(LED_PIN, HIGH);
    } else {
        printf("LED ON SLOW! (You lost!)\n");
        // Here, simulate a slow LED action for losing
        // Example: gpioWrite(LED_PIN, LOW);
    }
}

void start_game(int n) {
    printf("Starting %d games...\n", n);
    for (int i = 0; i < n; i++) {
        printf("Game %d started!\n", i+1);

        uint8_t a_sequence = generate_bit_sequence(3);
        uint8_t b_sequence = generate_bit_sequence(3);
        uint8_t game_sequence = generate_bit_sequence(game_length);

        print_bit_sequence(game_sequence, game_length);
        print_bit_sequence(a_sequence, 3);
        print_bit_sequence(b_sequence, 3);

        char winner = get_winner(a_sequence, b_sequence, game_sequence);
        if (winner == 'A') {
            a_wins++;
            gpio_feedback(1); // Client A wins, trigger fast LED feedback
        } else if (winner == 'B') {
            b_wins++;
            gpio_feedback(0); // Client B wins, trigger slow LED feedback
        }

        games_played++;
        printf("Winner: Player %c\n", winner);

        total_tosses += game_length;
    }
}

void print_game_statistics() {
    printf("\nGame Statistics:\n");
    printf("Number of players: %d\n", total_players);
    printf("Pattern length: %d\n", 3);
    printf("Games played: %d\n", games_played);
    printf("A wins: %.2f\n", a_wins);
    printf("B wins: %.2f\n", b_wins);
    printf("Winning probabilities:\n");
    printf("  Player A: %.2f%%\n", (a_wins / games_played) * 100);
    printf("  Player B: %.2f%%\n", (b_wins / games_played) * 100);
    printf("Average number of tosses: %.2f\n", (float)total_tosses / games_played);
}

int main() {
    srand(time(NULL));
    seeded = 1;

    start_game(5); // Start 5 games

    print_game_statistics();

    return 0;
}

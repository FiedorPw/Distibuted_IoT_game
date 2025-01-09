#include "includes.h"
#include <stdint.h>
#include <stdio.h>


static int seeded = 0;

// tak jakby array[n] tylko że dla bitów i na odwrót(bierze od końca)
int get_nth_bit(uint8_t sequence, int n){
    return (sequence >> n) & 1;
}

// generats random game sequence
uint8_t generate_bit_sequence(int lenght){
    uint8_t sequence = 0;
    for (int i = 0; i < lenght; i++){
        // Shift the sequence left by 1 bit to make room for the new bit
        sequence = sequence << 1;
        // Generate a random bit (0 or 1) and add it to the sequence using bitwise OR
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
    char winner = 'R'; //remis
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

int main(){
    srand(time(NULL));
    seeded = 1;
    uint8_t a_sequence = generate_bit_sequence(3);
    uint8_t b_sequence = generate_bit_sequence(3);
    uint8_t game_sequence = generate_bit_sequence(8);
    print_bit_sequence(game_sequence, 8);
    print_bit_sequence(a_sequence, 3);
    print_bit_sequence(b_sequence, 3);
    //test game
    char winner = get_winner(a_sequence, b_sequence, game_sequence);
    printf("%c\n", winner);

    return 0;
}

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>

#define SERVER_PORT 1337
#define BUFFER_SIZE 255
#define MAX_THROWS 100 // Maksymalna liczba rzutów w grze
#define MAX_SEQUENCE_LENGTH (MAX_THROWS * 2) // Maksymalna długość sekwencji (uwzględniając przecinki)
#define SEQUENCE_LENGTH 4 // Stała długość sekwencji klienta

// Definicje nagłówków komunikatów
#define START_HEADER  0x01
#define WINNER_HEADER 0x02
#define REGISTER_HEADER 0x03
#define GAME_HEADER   0x04
#define ACK_CLIENT_HEADER 0x05
#define ACK_SERVER_HEADER 0x06

// Kody wyniku dla wiadomości WINNER
#define WIN_RESULT  0x01
#define LOSE_RESULT 0x00

// Wyniki rzutu dla wiadomości GAME
#define HEADS 0x00
#define TAILS 0x01

// Struktura do przechowywania informacji o kliencie
typedef struct {
    struct sockaddr_in address;
    uint8_t id; // Zmieniono na uint8_t
    bool registered;
    int wins;
} ClientInfo;

// Struktura do przechowywania informacji o grze
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
    int *winning_throws_per_game; // Dodano: tablica przechowująca liczbę rzutów dla każdej gry
} GameInfo;

// Zmienne globalne
// static int seeded = 0; // Przeniesiona do funkcji get_random_number
GameInfo game;

// Funkcja generująca losową liczbę 0 lub 1
int get_random_number() {
    static int seeded = 0; // Teraz zmienna statyczna lokalna
    if (!seeded) {
        srand(time(NULL));
        seeded = 1;
    }
    return rand() % 2;
}

// Funkcja do znalezienia indeksu klienta w tablicy players
int find_player_index(struct sockaddr_in *client_addr) {
    for (int i = 0; i < game.num_players; i++) {
        if (game.players[i].registered &&
            game.players[i].address.sin_addr.s_addr == client_addr->sin_addr.s_addr &&
            game.players[i].address.sin_port == client_addr->sin_port) {
            return i;
        }
    }
    return -1; // Klient nie znaleziony
}

// Funkcja do znalezienia wolnego indeksu dla nowego klienta w tablicy players
int find_free_player_index() {
    for (int i = 0; i < game.num_players; i++) {
        if (!game.players[i].registered) {
            return i;
        }
    }
    return -1; // Brak wolnych miejsc
}

void send_message(int server_socket, uint8_t header, const uint8_t *message, int message_len, struct sockaddr_in *dest_addr, socklen_t addr_len) {
    uint8_t buffer[BUFFER_SIZE];
    buffer[0] = header;
    if (message != NULL && message_len > 0) {
        if (message_len > BUFFER_SIZE - 1) {
            printf("\033[31mBłąd: Wiadomość za długa.\033[0m\n");
            return;
        }
        memcpy(buffer + 1, message, message_len);
    }

    if (dest_addr != NULL) {
        char header_str[6]; // Zwiększono rozmiar bufora
        sprintf(header_str, "0x%02X", header);
        printf("\033[34mWysyłanie %s do %s:%d\033[0m\n", header_str, inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));

        if (sendto(server_socket, buffer, (message_len + 1), 0, (struct sockaddr *)dest_addr, addr_len) == -1) {
            perror("Błąd sendto");
        }
    } else {
        printf("\033[31mBłąd: Adres docelowy nie jest ustawiony.\033[0m\n");
    }
}
void send_ack(int server_socket, uint8_t received_header, struct sockaddr_in *dest_addr, socklen_t addr_len) {
    uint8_t buffer[2];
    buffer[0] = ACK_SERVER_HEADER;
    buffer[1] = received_header;

    if (dest_addr != NULL) {
        printf("\033[34mWysyłanie ACK dla 0x%02X do %s:%d\033[0m\n", received_header, inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));
        if (sendto(server_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)dest_addr, addr_len) == -1) {
            perror("Błąd sendto (ACK)");
        }
    } else {
        printf("\033[31mBłąd: Adres docelowy nie jest ustawiony.\033[0m\n");
    }
}
// Funkcja wysyłająca wiadomość W0 do wszystkich klientów
void send_winner_notification(int server_socket) {
    uint8_t buffer[2];
    buffer[0] = WINNER_HEADER;
    buffer[1] = LOSE_RESULT;
    for (int i = 0; i < game.num_players; i++) {
        if (game.players[i].registered) {
            send_message(server_socket, buffer[0], &buffer[1], 1, &game.players[i].address, sizeof(game.players[i].address));
        }
    }
    printf("Wysłano W0 (koniec gry) do wszystkich graczy.\n");
}

void send_game_message_to_all(int server_socket) {
    uint8_t server_num = get_random_number();
    uint8_t buffer[2];
    buffer[0] = GAME_HEADER;
    buffer[1] = (server_num == 0) ? HEADS : TAILS; // Poprawiony warunek

    // Dodanie rzutu do sekwencji
    if (game.throws > 0) {
        // Przygotowanie miejsca na przecinek i znak rzutu
        if (strlen(game.current_sequence) + 2 < MAX_SEQUENCE_LENGTH) {
            strcat(game.current_sequence, ",");
            sprintf(game.current_sequence + strlen(game.current_sequence), "%d", server_num);
        } else {
            // Obsługa przypadku, gdy sekwencja jest za długa
            printf("Osiągnięto maksymalną długość sekwencji.\n");
            return;
        }
    } else {
        // Pierwszy rzut, bez przecinka
        if (strlen(game.current_sequence) + 1 < MAX_SEQUENCE_LENGTH) {
            sprintf(game.current_sequence + strlen(game.current_sequence), "%d", server_num);
        } else {
            // Obsługa przypadku, gdy sekwencja jest za długa
            printf("Osiągnięto maksymalną długość sekwencji.\n");
            return;
        }
    }

    for (int i = 0; i < game.num_players; i++) {
        if (game.players[i].registered) {
            send_message(server_socket, buffer[0], &buffer[1], 1, &game.players[i].address, sizeof(game.players[i].address));
        }
    }

    printf("Wysłano %d do wszystkich graczy. game.throws: %d, Sekwencja: %s\n", server_num, game.throws, game.current_sequence); // Poprawiony komunikat
    game.throws++;
    game.last_game_message_time = time(NULL);
}

// Funkcja obsługująca rejestrację klienta
void handle_registration(int server_socket, char *buffer, struct sockaddr_in *client_addr, socklen_t client_len) {
    printf("\033[32mOdebrano żądanie rejestracji\033[0m\n");

    if (game.registered_players >= game.num_players) {
        printf("Gra już pełna.\n");
        return;
    }

    int free_index = find_free_player_index();
    if (free_index == -1) {
        printf("Brak wolnych miejsc dla nowych klientów.\n");
        return;
    }

    // Parsowanie danych rejestracyjnych
    uint8_t client_id = buffer[1]; // ID klienta
    struct in_addr client_ip; // Adres IP klienta
    client_ip.s_addr = *(uint32_t *)&buffer[2]; // Kopiowanie 4 bajtów adresu IP
    uint16_t client_port; // Port klienta
    memcpy(&client_port, &buffer[6], 2); // Kopiowanie 2 bajtów portu
    client_port = ntohs(client_port); // Konwersja portu z formatu sieciowego na hosta

    char client_ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_ip), client_ip_str, INET_ADDRSTRLEN);

    game.players[free_index].address = *client_addr;
    game.players[free_index].id = client_id;
    game.players[free_index].registered = true;
    game.players[free_index].wins = 0;
    game.registered_players++;

    printf("Zarejestrowano klienta ID: %u, IP: %s, Port: %u\n", client_id, client_ip_str, client_port);
    send_ack(server_socket, REGISTER_HEADER, client_addr, client_len);

    // Sprawdzenie, czy wszyscy gracze są już zarejestrowani
    if (game.registered_players == game.num_players) {
        printf("\033[32mWszyscy gracze zarejestrowani. Rozpoczynanie gry.\033[0m\n");
        game.game_started = true;
        game.throws = 0;
        strcpy(game.result, "");
        game.last_game_message_time = 0;
        game.current_game_num = 1;

        // Wysłanie START do wszystkich graczy
        for (int i = 0; i < game.num_players; i++) {
            send_message(server_socket, START_HEADER, NULL, 0, &game.players[i].address, sizeof(game.players[i].address));
        }
    }
}

// Funkcja obsługująca komunikat WINNER
void handle_winner_message(int server_socket, char *buffer, struct sockaddr_in *client_addr, socklen_t client_len) {
    int client_index = find_player_index(client_addr);
    if (client_index == -1) {
        printf("Nieznany klient wysłał wiadomość WINNER.\n");
        return;
    }

    printf("\033[32mOdebrano komunikat WINNER od klienta ID %u: 0x%02X\033[0m\n", game.players[client_index].id, buffer[1]);

    if (game.game_started) {
        if (buffer[1] == WIN_RESULT) {
            printf("Ktoś wygrał!\n");
            strcpy(game.result, "W");
            game.game_started = false;
            game.players[client_index].wins++; // Zwiększenie liczby wygranych gier dla zwycięzcy
            // Zapisanie zwycięskiej sekwencji i liczby rzutów
            strcpy(game.winning_sequence, game.current_sequence);
            game.winning_throws_per_game[game.current_game_num - 1] = game.throws;
        } else if (buffer[1] == LOSE_RESULT) {
            printf("Ktoś przegrał!\n");
            strcpy(game.result, "P");
            game.game_started = false;
             // Zapisanie zwycięskiej sekwencji i liczby rzutów
            strcpy(game.winning_sequence, game.current_sequence);
            game.winning_throws_per_game[game.current_game_num - 1] = game.throws;

} else {
            printf("Niepoprawna tresc w komunikacie WINNER");
        }

        // Niezależnie od wyniku, wysyłamy W0 i rozpoczynamy nową grę (jeśli to nie była ostatnia gra)
        send_winner_notification(server_socket);

        if (game.current_game_num < game.total_games) {
            game.current_game_num++;
            printf("\033[32mRozpoczynanie gry %d\033[0m\n", game.current_game_num);
            game.game_started = true;
            game.throws = 0;
            strcpy(game.result, "");
            strcpy(game.current_sequence, ""); // Wyczyszczenie sekwencji rzutów
            game.last_game_message_time = 0;

            // Wysłanie START do wszystkich graczy
            for (int i = 0; i < game.num_players; i++) {
                send_message(server_socket, START_HEADER, NULL, 0, &game.players[i].address, sizeof(game.players[i].address));
            }
        } else {
            printf("\033[32mWszystkie gry zostały rozegrane.\033[0m\n");
            // Wyswietlenie statystyk
            for(int i = 0; i < game.num_players; i++) {
                printf("Gracz %d wygrał %d razy.\n", game.players[i].id, game.players[i].wins);
                if (game.total_games > 0) {
                    double win_ratio = (double)game.players[i].wins / game.total_games;
                    printf("Szansa na wygraną gracza %d: %.2f%%\n", game.players[i].id, win_ratio * 100);
                }
            }
            // Wyświetlenie statystyk każdej gry
            printf("\nStatystyki gier:\n");
            int total_throws = 0; // Deklaracja zmiennej total_throws
            for (int i = 1; i <= game.total_games; i++) {
                printf("Gra %d: Liczba rzutów: %d", i, game.winning_throws_per_game[i-1]);
                total_throws += game.winning_throws_per_game[i-1];

                // Wyświetlenie fragmentu zwycięskiej sekwencji
                int start_index = 0;
                int sequence_length = 0;

                // Tworzymy kopię winning_sequence, aby nie modyfikować oryginału
                char temp_winning_sequence[MAX_SEQUENCE_LENGTH];
                strcpy(temp_winning_sequence, game.winning_sequence);

                // Obliczamy długość sekwencji i indeks startowy
                char *token = strtok(temp_winning_sequence, ",");
                while (token != NULL) {
                    sequence_length++;
                    token = strtok(NULL, ",");
                }

                if (sequence_length > SEQUENCE_LENGTH) {
                    start_index = sequence_length - SEQUENCE_LENGTH;
                }

                // Ponownie kopiujemy oryginalna sekwencje
                strcpy(temp_winning_sequence, game.winning_sequence);

                // Przechodzimy do indeksu startowego
                token = strtok(temp_winning_sequence, ",");
                for (int j = 0; j < start_index; j++) {
                    token = strtok(NULL, ",");
                }

                printf(", Zwycięska sekwencja: ");
                for (int j = 0; j < SEQUENCE_LENGTH; j++) {
                    if (token != NULL) {
                        printf("%s", token);
                        token = strtok(NULL, ",");
                        if (j < SEQUENCE_LENGTH - 1 && token != NULL)
                            printf(",");
                    }
                }
                printf("\n");
            }
            // Obliczenie i wyświetlenie średniej liczby rzutów na grę
            if (game.total_games > 0) {
                double average_throws = (double)total_throws / game.total_games;
                printf("Średnia liczba rzutów na grę: %.2f\n", average_throws);
            }

        }
    }
    else
       send_ack(server_socket, WINNER_HEADER, client_addr, client_len);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Użycie: %s <liczba_graczy> <liczba_gier>\n", argv[0]);
        return 1;
    }

    game.num_players = atoi(argv[1]);
    game.total_games = atoi(argv[2]);

    game.players = (ClientInfo *)malloc(game.num_players * sizeof(ClientInfo));
    if (game.players == NULL) {
        perror("Błąd alokacji pamięci dla game.players");
        return 1;
    }
    game.winning_throws_per_game = (int *)malloc(game.total_games * sizeof(int));
    if (game.winning_throws_per_game == NULL) {
        perror("Błąd alokacji pamięci dla game.winning_throws_per_game");
        return 1;
    }

    for (int i = 0; i < game.num_players; i++) {
        game.players[i].registered = false;
        game.players[i].wins = 0;
    }
    game.registered_players = 0;
    game.game_started = false;
    memset(game.current_sequence, 0, sizeof(game.current_sequence));
    memset(game.winning_sequence, 0, sizeof(game.winning_sequence));

    printf("Uruchamianie serwera z ID %d, liczba graczy: %d, liczba gier: %d\n", game.num_players, game.total_games);

    int server_socket;
    struct sockaddr_in server_addr;

    // Utworzenie gniazda UDP
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        perror("Błąd tworzenia gniazda");
        return 1;
    }

    // Konfiguracja adresu serwera
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Błąd bind");
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
        tv.tv_usec = 500000; // 0.5 sekundy

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0) {
            perror("Błąd select");
            printf("Wartość errno: %d\n", errno);
            break;
        }

        if (FD_ISSET(server_socket, &readfds)) {
            char buffer[BUFFER_SIZE];
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            ssize_t recv_len = recvfrom(server_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);

            if (recv_len < 0) {
                perror("Błąd recvfrom");
                printf("Wartość errno: %d\n", errno);
                continue;
            }

            if (recv_len > 0) {
                uint8_t header = buffer[0];
                printf("\033[35mOtrzymano wiadomość o nagłówku: 0x%02X\033[0m\n", header);

                switch (header) {
                    case REGISTER_HEADER:
                        if(recv_len == 8){
                            handle_registration(server_socket, buffer, &client_addr, client_len);
                        } else {
                            printf("Nieprawidłowa długość wiadomości REGISTER.\n");
                        }
                        break;
                    case ACK_CLIENT_HEADER:
                        printf("\033[32mOtrzymano ACK od klienta.\033[0m\n");
                        break;
                    case WINNER_HEADER:
                        if(recv_len == 2){
                            handle_winner_message(server_socket, buffer, &client_addr, client_len);
                        } else {
                             printf("Nieprawidłowa długość wiadomości WINNER.\n");
                        }

                        break;
                    default:
                        printf("Nieznany nagłówek: 0x%02X\n", header);
                }
            }
        } else {
            // Logika gry - wysyłanie GAME co sekundę, jeśli gra się rozpoczęła i nie została zakończona
            if (game.game_started && game.throws < MAX_THROWS && strlen(game.result) == 0) {
                if (time(NULL) - game.last_game_message_time >= 1) {
                    send_game_message_to_all(server_socket);
                }
            }
        }
        // Sprawdzenie, czy rozegrano wszystkie gry
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

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

#define SERVER_PORT 1305
#define BUFFER_SIZE 255

// Definicje nagłówków komunikatów
#define START 'S'
#define WINNER 'W'
#define REGISTER 'R'
#define GAME 'G'
#define ACK 'a'

// Struktura do przechowywania informacji o kliencie
typedef struct {
    struct sockaddr_in address;
    int id;
    bool registered;
    bool game_started;
    int wins;
    int games_played;
} ClientInfo;

// Struktura do przechowywania statystyk gry
typedef struct {
    int game_id;
    int throws;
    char result[2];
} GameStats;


// Zmienne globalne
static int seeded = 0;
int server_id;
int num_games;
int current_game = 0;
ClientInfo client = {0};
GameStats *game_stats;
bool game_won = false; // Flaga informująca o wygranej klienta


// Funkcja generująca losową liczbę 0 lub 1
int get_random_number() {
    if (!seeded) {
        srand(time(NULL) + server_id);
        seeded = 1;
    }
    return rand() % 2;
}

void print_game_stats() {
    printf("\n----- Statystyki Gier -----\n");
    for (int i = 0; i < current_game; i++) {
        printf("Gra %d: Liczba rzutów: %d, Wynik: %s\n",
               game_stats[i].game_id, game_stats[i].throws, game_stats[i].result);
    }
    if (client.games_played > 0) {
        double win_percentage = (double)client.wins / client.games_played * 100;
        printf("Procent wygranych klienta: %.2f%%\n", win_percentage);
    }
    printf("---------------------------\n");
}

// Funkcja wysyłająca wiadomość
void send_message(int server_socket, char header, const char *message, struct sockaddr_in *dest_addr, socklen_t addr_len) {
    char buffer[BUFFER_SIZE];
    buffer[0] = header;
    if (message != NULL) {
        strncpy(buffer + 1, message, BUFFER_SIZE - 2);
        buffer[BUFFER_SIZE - 1] = '\0';
    } else {
        buffer[1] = '\0';
    }
    
    
    if (dest_addr != NULL) {
      printf("\033[34mWysyłanie %c: %s do %s:%d\033[0m\n", header, message, inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));
      sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)dest_addr, addr_len);
    } else {
        printf("\033[31mBłąd: Adres docelowy nie jest ustawiony.\033[0m\n");
    }
}

void send_ack(int server_socket, char received_header, struct sockaddr_in *dest_addr, socklen_t addr_len) {
    char ack_message[2] = {received_header, '\0'};
    send_message(server_socket, ACK, ack_message, dest_addr, addr_len);
}


// Funkcja obsługująca rejestrację klienta
void handle_registration(int server_socket, char *message, struct sockaddr_in *client_addr, socklen_t client_len) {
    printf("\033[32mOdebrano żądanie rejestracji\033[0m\n");

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr->sin_port);

    if (sscanf(message, "%[^,],%d,%d", client_ip, &client_port, &client.id) == 3) {
        client.address = *client_addr;
        client.registered = true;
        printf("Zarejestrowano klienta ID: %d, IP: %s, Port: %d\n", client.id, client_ip, client_port);
        send_ack(server_socket, REGISTER, client_addr, client_len);
    } else {
        printf("Błąd podczas parsowania danych rejestracyjnych.\n");
    }
}

// Funkcja wysyłająca wiadomość GAME
void send_game_message(int server_socket, struct sockaddr_in *client_addr, socklen_t client_len) {
    char server_num[2];
    server_num[0] = get_random_number() + '0';
    server_num[1] = '\0';

    send_message(server_socket, GAME, server_num, client_addr, client_len);
    printf("Wysłano %s do klienta.\n", server_num);

    game_stats[current_game].throws++;
}

void handle_winner_message(int server_socket, char *message, struct sockaddr_in *client_addr, socklen_t client_len) {
    printf("\033[32mOdebrano komunikat WINNER od klienta: %s\033[0m\n", message);
    //printf("Wartość message[0] w handle_winner_message: %c\n", message[0]); // Dodano
    //printf("Wartość game_won przed przetworzeniem WINNER: %d\n", game_won); // Dodano
    //printf("Wartość client.game_started przed przetworzeniem WINNER: %d\n", client.game_started); // Dodano

    if (message[0] == '1') {
        printf("Klient wygrał!\n");
        client.wins++;
        strcpy(game_stats[current_game].result, "W");
        game_won = true;
    } else if (message[0] == '0'){
        printf("Klient przegrał!\n");
        strcpy(game_stats[current_game].result, "P");
        game_won = true;
    } else {
        printf("Niepoprawny wynik w komunikacie WINNER!\n");
    }
    
    game_stats[current_game].game_id = current_game + 1;
    client.games_played++;
    current_game++;
    client.game_started = false;

    if (current_game >= num_games) {
        printf("Wszystkie gry zostały rozegrane.\n");
        print_game_stats();
        client.registered = false;
        client.games_played = 0;
        client.wins = 0;
    }

    send_ack(server_socket, WINNER, client_addr, client_len);
    // Ustawienie flagi wygranej PO wysłaniu ACK - przeniesione tutaj w poprzedniej poprawce
    printf("Wartosc game_won po przetworzeniu WINNER: %d\n", game_won); // Dodano
    printf("Wartosc client.game_started po przetworzeniu WINNER: %d\n", client.game_started); // Dodano
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Użycie: %s <id_serwera> <liczba_gier>\n", argv[0]);
        return 1;
    }

    server_id = atoi(argv[1]);
    num_games = atoi(argv[2]);
    game_stats = malloc(num_games * sizeof(GameStats));

    printf("Uruchamianie serwera z ID %d, liczba gier: %d\n", server_id, num_games);

    int server_socket;
    struct sockaddr_in server_addr;
    socklen_t client_len = sizeof(client.address);

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
    
    // Zmienna do opóźnienia wysyłania GAME
    unsigned long last_game_message_time = 0;
    
    while (1) {
        //printf("Pętla while(1) - iteracja\n");
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);

        tv.tv_sec = 0;
        tv.tv_usec = 500000;

        //printf("Przed select(), tv_sec: %ld, tv_usec: %ld\n", tv.tv_sec, tv.tv_usec);
        int activity = select(server_socket + 1, &readfds, NULL, NULL, &tv);
        //printf("Po select(), activity: %d\n", activity);

        if (activity < 0) {
            perror("Błąd select");
            //printf("Wartość errno: %d\n", errno);
            break;
        }

        if (FD_ISSET(server_socket, &readfds)) {
            //printf("FD_ISSET jest spełniony!\n");
            char buffer[BUFFER_SIZE];
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            ssize_t recv_len = recvfrom(server_socket, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &client_len);

            if (recv_len < 0) {
                perror("Błąd recvfrom");
                //printf("Wartość errno: %d\n", errno);
                continue;
            }
            //printf("Odebrano %zd bajtow\n", recv_len);
            //printf("Odebrano z: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            buffer[recv_len] = '\0';
            char header = buffer[0];
            char *message = buffer + 1;

            printf("\033[35mOtrzymano wiadomość: [%c]%s\033[0m\n", header, message);

            switch (header) {
                case REGISTER:
                    if (!client.registered) {
                        handle_registration(server_socket, message, &client_addr, client_len);
                        // Rozpocznij grę zaraz po rejestracji, wysyłając START
                        if (client.registered && current_game < num_games) {
                            printf("\033[32mRozpoczynanie gry dla klienta ID: %d\033[0m\n", client.id);
                            client.game_started = true;
                            game_stats[current_game].throws = 0;
                            game_won = false;
                            send_message(server_socket, START, NULL, &client.address, client_len);
                        }
                    } else {
                        printf("Klient już zarejestrowany.\n");
                        send_ack(server_socket, REGISTER, &client_addr, client_len);
                    }
                    break;
                case ACK:
                    printf("\033[32mOtrzymano ACK dla: %s\033[0m\n", message);
                    // Sprawdź, czy w buforze są jeszcze dane (po ACK)
                    if (strlen(message) > 1) {
                        // Przesuń wskaźnik message, aby wskazywał na kolejny nagłówek
                        message++; 
                        if (message[0] == WINNER) {
                            printf("Odebrano WINNER po ACK\n");
                            handle_winner_message(server_socket, message + 1, &client_addr, client_len);
                        }
                    }
                    break;
                case WINNER:
                    printf("Odebrano komunikat:  WINNER\n");
                    handle_winner_message(server_socket, message, &client_addr, client_len);
                    break;

                default:
                    printf("Nieznany nagłówek: %c\n", header);
            }
        } else {
            //printf("Nie wykryto aktywności na sockecie (timeout).\n");
             // Sprawdzenie, czy gra się rozpoczęła i czy klient jest zarejestrowany
            if (client.game_started && client.registered && !game_won) {
                // Sprawdzenie, czy minęła 1 sekunda od ostatniej wysłanej wiadomości GAME
                if (time(NULL) - last_game_message_time >= 1) {
                    send_game_message(server_socket, &client.address, client_len);
                    last_game_message_time = time(NULL);
                }
            }
        }
        
        // Sprawdzenie czy gra się skończyła
        if (game_won) {
            // Rozpocznij nową grę, jeśli nie osiągnięto limitu
            if (client.registered && current_game < num_games) {
                printf("\033[32mRozpoczynanie nowej gry dla klienta ID: %d\033[0m\n", client.id);
                client.game_started = true;
                game_stats[current_game].throws = 0;
                game_won = false;
                send_message(server_socket, START, NULL, &client.address, client_len);
            }
            game_won = false;
        }
    }

    close(server_socket);
    free(game_stats);
    return 0;
}

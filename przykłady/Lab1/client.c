#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>     // Biblioteka zawierająca funkcje do obsługi socketów
#include <arpa/inet.h>      // Biblioteka dla operacji internetowych
#include <unistd.h>         // Dla funkcji close() i sleep()
#include <time.h>           // Dla funkcji time() - obsługa czasu
#include <sys/time.h>       // Dla funkcji gettimeofday() - precyzyjny pomiar czasu

// Stałe konfiguracyjne
#define CLIENT_PORT 1305    // Port nasłuchiwania klienta
#define BUFFER_SIZE 1024    // Rozmiar bufora na wiadomości
#define PING_MESSAGE_LEN 13 // Długość wiadomości PING wysyłanej do serwera
#define MAX_SERVERS 10      // Maksymalna liczba serwerów w tablicy

#define MAX_IP_LENGTH 16    // Maksymalna długość adresu IP (format XXX.XXX.XXX.XXX\0)
#define UP 1               // Status serwera - aktywny
#define DOWN 0             // Status serwera - nieaktywny

// Nagłówki protokołu komunikacyjnego
#define HELLO 'h'    // Wiadomość powitalna
#define PING 'i'     // Żądanie ping
#define PONG 'o'     // Odpowiedź na ping
#define REQUEST 'q'  // Sprawdzenie aktywności
#define RESPONSE 's' // Potwierdzenie aktywności

#define REQUEST_INTERVAL 0.27  // Interwał sprawdzania aktywności (270ms)
#define MAX_REQUEST_ATTEMPTS 3 // Maksymalna liczba prób przed uznaniem serwera za nieaktywny

// Zmienna do inicjalizacji generatora liczb losowych
static int seeded = 0;

// Struktura przechowująca informacje o serwerze
// Używa typów wbudowanych w C do śledzenia stanu serwera
struct ServerInfo {
    int id;                     // Identyfikator serwera
    char ip[MAX_IP_LENGTH];     // Tablica znaków na adres IP (statyczna alokacja)
    int port;                   // Numer portu
    int status;                 // Status (UP/DOWN)
    int failed_requests;        // Licznik nieudanych prób połączenia
    time_t last_request_time;   // Znacznik czasu ostatniego żądania (sekundy)
    long last_request_time_usec; // Mikrosekundy ostatniego żądania
};

// Struktura do śledzenia stanu ping-pong
struct PingInfo {
    struct timeval start_time;   // Struktura czasu z sys/time.h
    int waiting_for_pong;       // Flag oczekiwania na odpowiedź
} ping_state = {.waiting_for_pong = 0};  // Inicjalizacja zmiennej globalnej

// Globalna tablica serwerów - pamięć alokowana statycznie
struct ServerInfo servers[MAX_SERVERS];
int server_count = 0;  // Licznik aktywnych serwerów

// Prototypy funkcji
char* add_header(char* message, char header);
char* process_header(char* message);
void server_hello_handler(char* message, const char* ip, int port);
char* generate_random_string(int length);
void print_servers();
void client_listen(int server_socket, struct sockaddr_in sender_addr, socklen_t client_len);
void init_random_generator_seed();
void send_pings(int client_socket);
int get_random_active_server();

// Funkcja sprawdzająca aktywność serwerów
void send_keep_alive_check(int client_socket) {
    struct timeval now;  // Struktura przechowująca aktualny czas
    gettimeofday(&now, NULL);  // Pobranie aktualnego czasu z mikrosekundami
    double current_time_ms = now.tv_sec + (now.tv_usec / 1000000.0);

    // Iteracja po wszystkich serwerach
    for(int i = 0; i < server_count; i++) {
        if(servers[i].status == UP) {
            // Obliczenie czasu od ostatniego sprawdzenia
            double time_since_last = current_time_ms -
                                   (servers[i].last_request_time +
                                    (servers[i].last_request_time_usec / 1000000.0));

            if(time_since_last >= REQUEST_INTERVAL) {
                // Przygotowanie struktury adresu serwera
                struct sockaddr_in server_addr;
                memset(&server_addr, 0, sizeof(server_addr));  // Wyzerowanie pamięci struktury
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(servers[i].port);
                inet_pton(AF_INET, servers[i].ip, &server_addr.sin_addr);

                char request = REQUEST;
                printf("\033[34mWysyłanie wiadomości: [%c]\033[0m\n", request);
                sendto(client_socket,
                      &request,
                      1,
                      0,
                      (struct sockaddr*)&server_addr,
                      sizeof(server_addr));

                printf("\033[33mWysłano REQUEST do serwera %d (próba %d)\033[0m\n",
                       servers[i].id, servers[i].failed_requests + 1);

                servers[i].last_request_time = now.tv_sec;
                servers[i].last_request_time_usec = now.tv_usec;
                servers[i].failed_requests++;

                if(servers[i].failed_requests >= MAX_REQUEST_ATTEMPTS) {
                    printf("\033[31mSerwer %d nie odpowiada, oznaczanie jako DOWN\033[0m\n",
                           servers[i].id);
                    servers[i].status = DOWN;
                    servers[i].failed_requests = 0;
                    print_servers();
                }
            }
        }
    }
}

// Funkcja obsługująca odpowiedź PONG
void handle_pong_response(const char* message, struct timeval* current_time) {
    struct timeval end_time = *current_time;
    if (ping_state.waiting_for_pong) {
        // Obliczenie czasu odpowiedzi (RTT) w milisekundach
        double rtt = (end_time.tv_sec - ping_state.start_time.tv_sec) * 1000.0 +
                    (end_time.tv_usec - ping_state.start_time.tv_usec) / 1000.0;

        time_t now = time(NULL);
        char* timestamp = ctime(&now);
        timestamp[strlen(timestamp)-1] = '\0';  // Usunięcie znaku nowej linii

        printf("\033[36mOtrzymano PONG: %s, RTT: %.3f ms, Czas: %s\033[0m\n",
               message,
               rtt,
               timestamp);

        ping_state.waiting_for_pong = 0;
    }
}

// Funkcja wybierająca losowy aktywny serwer
int get_random_active_server() {
    if (server_count == 0) return -1;

    int active_servers = 0;
    for(int i = 0; i < server_count; i++) {
        if(servers[i].status == UP) {
            active_servers++;
        }
    }

    if (active_servers == 0) return -1;

    int target = rand() % active_servers;
    int current = 0;

    for(int i = 0; i < server_count; i++) {
        if(servers[i].status == UP) {
            if(current == target) {
                return i;
            }
            current++;
        }
    }

    return -1;
}

// Funkcja wysyłająca wiadomość PING do losowego serwera
void send_pings(int client_socket) {
    int server_index = get_random_active_server();

    if(server_index == -1) {
        printf("Brak aktywnych serwerów\n");
        return;
    }

    // Dynamiczna alokacja pamięci dla wiadomości
    char* message = generate_random_string(PING_MESSAGE_LEN);
    char* message_with_header = add_header(message, PING);

    printf("\033[34mWysyłanie wiadomości: [%c]%s\033[0m\n",
            PING, message);

    gettimeofday(&ping_state.start_time, NULL);
    ping_state.waiting_for_pong = 1;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(servers[server_index].port);
    inet_pton(AF_INET, servers[server_index].ip, &server_addr.sin_addr);

    printf("\033[34mWysyłanie PING do serwera %d (IP: %s, Port: %d): %s\033[0m\n",
           servers[server_index].id,
           servers[server_index].ip,
           servers[server_index].port,
           message_with_header);

    sendto(client_socket,
           message_with_header,
           strlen(message_with_header),
           0,
           (struct sockaddr*)&server_addr,
           sizeof(server_addr));

    // Zwalnianie zaalokowanej pamięci
    free(message);
    free(message_with_header);
}
// Funkcja obsługująca wiadomości HELLO od serwerów
// Parametry:
// message - wskaźnik na ciąg znaków zawierający wiadomość (pamięć dynamiczna)
// ip - stały wskaźnik na ciąg znaków z adresem IP (nie będzie modyfikowany)
// port - numer portu jako liczba całkowita
void server_hello_handler(char* message, const char* ip, int port) {
    // Wyciągnięcie ID serwera z wiadomości tekstowej przy użyciu sscanf
    // sscanf analizuje tekst i próbuje znaleźć liczbę całkowitą
    int server_id;
    if (sscanf(message, "%d", &server_id) != 1) {
        printf("Nie udało się odczytać ID serwera\n");
        return;
    }

    // Sprawdzenie czy serwer już istnieje w tablicy serwerów
    for(int i = 0; i < server_count; i++) {
        if(servers[i].id == server_id) {
            // Aktualizacja danych istniejącego serwera
            // strcpy kopiuje ciąg znaków do bufora o określonym rozmiarze
            strcpy(servers[i].ip, ip);
            servers[i].port = port;
            servers[i].status = UP;
            printf("Zaktualizowano serwer %d\n", server_id);
            return;
        }
    }

    // Dodanie nowego serwera jeśli jest miejsce w tablicy
    if(server_count < MAX_SERVERS) {
        // Wypełnienie struktury ServerInfo danymi nowego serwera
        servers[server_count].id = server_id;
        strcpy(servers[server_count].ip, ip);
        servers[server_count].port = port;
        servers[server_count].status = UP;
        servers[server_count].failed_requests = 0;
        servers[server_count].last_request_time = time(NULL);
        servers[server_count].last_request_time_usec = 0;
        printf("Dodano nowy serwer %d o IP: %s Port: %d\n",
               server_id, ip, port);
        server_count++;
    } else {
        printf("Lista serwerów pełna!\n");
    }
}

// Funkcja wyświetlająca listę wszystkich znanych serwerów
void print_servers() {
    printf("\nZnane serwery:\n");
    for(int i = 0; i < server_count; i++) {
        printf("ID serwera: %d, IP: %s, Port: %d, Status: %s\n",
               servers[i].id,
               servers[i].ip,
               servers[i].port,
               servers[i].status ? "AKTYWNY" : "NIEAKTYWNY");
    }
    printf("\n");
}

// Główna funkcja nasłuchująca na wiadomości od serwerów
// Obsługuje odbiór pakietów UDP i ich przetwarzanie
void client_listen(int server_socket, struct sockaddr_in sender_addr, socklen_t client_len) {
    // Bufor na dane przychodzące - tablica znaków alokowana na stosie
    char buffer[BUFFER_SIZE];
    socklen_t sender_len = sizeof(sender_addr);
    struct timeval recv_time;  // Struktura na czas otrzymania pakietu

    // Odebranie pakietu UDP
    int recv_len = recvfrom(server_socket,
                           buffer,
                           BUFFER_SIZE,
                           0,
                           (struct sockaddr*)&sender_addr,
                           &client_len);

    if (recv_len > 0) {
        // Natychmiastowy pomiar czasu otrzymania
        gettimeofday(&recv_time, NULL);

        // Dodanie terminatora null na końcu bufora
        buffer[recv_len] = '\0';

        // Konwersja adresu IP nadawcy z formatu binarnego na tekstowy
        char sender_ip[MAX_IP_LENGTH];
        inet_ntop(AF_INET, &(sender_addr.sin_addr), sender_ip, MAX_IP_LENGTH);
        int sender_port = ntohs(sender_addr.sin_port);

        // Wyodrębnienie nagłówka i wiadomości
        char header = buffer[0];
        // Dynamiczna alokacja pamięci na wiadomość
        char* message = malloc(recv_len);  // Pamięć będzie zwolniona na końcu funkcji
        strcpy(message, buffer + 1);

        printf("\033[35mOtrzymano wiadomość: [%c]%s\033[0m\n",
                       header, buffer + 1);

        // Obsługa różnych typów wiadomości
        switch(header) {
            case HELLO:
                printf("\033[32mOtrzymano wiadomość HELLO\033[0m\n");
                server_hello_handler(message, sender_ip, sender_port);
                // Resetowanie licznika nieudanych prób i ustawienie statusu na AKTYWNY
                for(int i = 0; i < server_count; i++) {
                    if(strcmp(servers[i].ip, sender_ip) == 0 &&
                       servers[i].port == sender_port) {
                        servers[i].failed_requests = 0;
                        servers[i].status = UP;
                        servers[i].last_request_time = time(NULL);
                        servers[i].last_request_time_usec = 0;
                        printf("\033[32mSerwer %d reaktywowany\033[0m\n", servers[i].id);
                        break;
                    }
                }
                print_servers();
                break;
            case PING:
                printf("\033[32mOtrzymano wiadomość PING: %s\033[0m\n", message);
                break;
            case PONG:
                handle_pong_response(message, &recv_time);
                // Aktualizacja statusu serwera
                for(int i = 0; i < server_count; i++) {
                    if(strcmp(servers[i].ip, sender_ip) == 0 &&
                       servers[i].port == sender_port) {
                        servers[i].status = UP;
                        break;
                    }
                }
                break;
            case REQUEST:
                printf("\033[32mOtrzymano wiadomość REQUEST: %s\033[0m\n", message);
                break;
            case RESPONSE:
                printf("\033[32mOtrzymano RESPONSE od %s:%d\033[0m\n",
                       sender_ip, sender_port);
                // Aktualizacja statusu serwera
                for(int i = 0; i < server_count; i++) {
                    if(strcmp(servers[i].ip, sender_ip) == 0 &&
                       servers[i].port == sender_port) {
                        servers[i].failed_requests = 0;
                        servers[i].status = UP;
                        break;
                    }
                }
                break;
            default:
                printf("\033[31mNieznany typ wiadomości: %c\033[0m\n", header);
        }

        // Zwolnienie zaalokowanej pamięci na wiadomość
        free(message);
    }
}

// Funkcja dodająca nagłówek do wiadomości
// Tworzy nową wiadomość w pamięci dynamicznej
char* add_header(char* message, char header) {
    if (message == NULL) {
        return NULL;
    }

    // Obliczenie długości nowej wiadomości
    size_t msg_len = strlen(message);
    size_t new_len = msg_len + 2;  // +1 na nagłówek, +1 na terminator null

    // Dynamiczna alokacja pamięci na nową wiadomość
    char* new_message = (char*)malloc(new_len);
    if (new_message == NULL) {
        return NULL;
    }

    // Dodanie nagłówka i skopiowanie wiadomości
    new_message[0] = header;
    strcpy(new_message + 1, message);  // Skopiowanie za nagłówkiem

    return new_message;  // Zwrócenie wskaźnika - pamięć musi być zwolniona przez wywołującego
}

// Inicjalizacja generatora liczb pseudolosowych
void init_random_generator_seed() {
    if (!seeded) {
        srand(time(NULL));  // Inicjalizacja ziarnem opartym na czasie
        seeded = 1;
    }
}

// Funkcja generująca losowy ciąg znaków określonej długości
char* generate_random_string(int length) {
    // Dynamiczna alokacja pamięci na ciąg znaków
    char* random_string = (char*)malloc(length + 1);

    const char charset[] = "0123456789"
                         "abcdefghijklmnopqrstuvwxyz";

    int charset_length = strlen(charset);

    // Wypełnienie bufora losowymi znakami
    for (int i = 0; i < length; i++) {
        int key = rand() % charset_length;
        random_string[i] = charset[key];
    }

    random_string[length] = '\0';  // Dodanie terminatora null

    return random_string;  // Zwrócenie wskaźnika - pamięć musi być zwolniona przez wywołującego
}

// Funkcja wysyłająca pakiet PING do serwera i obsługująca odpowiedź
// Argumenty:
// - client_socket: deskryptor gniazda UDP (int)
// - server_addr: struktura zawierająca adres serwera (struct sockaddr_in)
// - server_len: rozmiar struktury adresu (socklen_t - specjalny typ dla rozmiaru adresu)
// - buffer: wskaźnik na bufor do przechowywania odpowiedzi (tablica char[])
void ping_server(int client_socket, struct sockaddr_in server_addr, socklen_t server_len, char *buffer){
    // Struktury przechowujące czas z mikrosekudnową dokładnością
    struct timeval start,end;
    double rtt;  // Zmienna na czas odpowiedzi (Round Trip Time)

    // Alokacja pamięci i generowanie losowej wiadomości
    char *message = generate_random_string(PING_MESSAGE_LEN);  // Dynamiczna alokacja pamięci

    if (message == NULL) {
            printf("Nie udało się wygenerować losowego ciągu\n");
            return;
    }

    // Dodanie nagłówka do wiadomości - zwraca nowy wskaźnik, stary zostanie zwolniony
    message = add_header(message,PING);

    printf("\033[34mWysyłanie: %s \033[0m\n", message);

    // Pomiar czasu startu z dokładnością do mikrosekund
    gettimeofday(&start, NULL);

    // Wysłanie wiadomości przez UDP używając struktury sockaddr_in
    sendto(client_socket,
           message,
           strlen(message),
           0,
           (struct sockaddr*)&server_addr,  // Rzutowanie na ogólną strukturę adresu
           sizeof(server_addr));

    // Oczekiwanie na odpowiedź i zapis do bufora
    int recv_len = recvfrom(client_socket,
                           buffer,
                           BUFFER_SIZE,
                           0,
                           (struct sockaddr*)&server_addr,
                           &server_len);
    if (recv_len > 0) {
            buffer[recv_len] = '\0';  // Dodanie terminatora null na końcu otrzymanych danych

            // Pomiar czasu końca i obliczenie RTT w milisekundach
            gettimeofday(&end,NULL);
            rtt = (double)(end.tv_usec - start.tv_usec) / 1000;

            // Pobranie aktualnego czasu (mniej dokładne niż gettimeofday)
            time_t current_time;
            time(&current_time);
            char *timestamp = ctime(&current_time);  // Konwersja czasu na string
            timestamp[strlen(timestamp)-1] = '\0';   // Usunięcie znaku nowej linii

            printf("Otrzymano odpowiedź: %s, RTT: %.3f ms, Czas: %s\n",
                   buffer + 1,  // Pominięcie pierwszego znaku (nagłówka)
                   rtt,
                   timestamp);
        }

    free(message);  // Zwolnienie pamięci zaalokowanej dla wiadomości
    sleep(1);       // Wstrzymanie wykonania na 1 sekundę
}

// Funkcja zwracająca losowy interwał w milisekundach
int get_random_ping_interval() {
    return 1500 + (rand() % 1051);  // Losowa liczba z zakresu 1500-2550ms
}

// Główna funkcja programu
int main() {
    printf("Klient\n");
    init_random_generator_seed();  // Inicjalizacja generatora liczb pseudolosowych

    // Deklaracja zmiennych do obsługi socketu UDP
    int client_socket;
    struct sockaddr_in server_addr, client_addr;    // Struktury przechowujące adresy IP i porty
    char buffer[BUFFER_SIZE];                       // Statyczna alokacja bufora na dane
    socklen_t server_len = sizeof(server_addr);     // Rozmiar struktury adresu

    // Utworzenie gniazda UDP
    client_socket = socket(AF_INET,     // Rodzina protokołów IPv4
                          SOCK_DGRAM,   // Typ gniazda - UDP
                          0);           // Protokół domyślny

    // Inicjalizacja struktury adresu klienta
    memset(&client_addr, 0, sizeof(client_addr));   // Wyzerowanie pamięci struktury
    client_addr.sin_family = AF_INET;               // Ustawienie rodziny na IPv4
    client_addr.sin_addr.s_addr = INADDR_ANY;       // Nasłuchiwanie na wszystkich interfejsach
    client_addr.sin_port = htons(CLIENT_PORT);      // Konwersja numeru portu na format sieciowy

    // Przypisanie adresu do gniazda
    if (bind(client_socket,
            (struct sockaddr*)&client_addr,
            sizeof(client_addr)) < 0) {
        perror("Błąd bind");
        exit(1);
    }

    // Zmienne do obsługi select() - mechanizmu multipleksowania we/wy
    fd_set readfds;           // Zestaw deskryptorów do monitorowania
    struct timeval tv;        // Struktura na timeout
    struct timeval last_ping_time;
    gettimeofday(&last_ping_time, NULL);
    int current_ping_interval = get_random_ping_interval();

    // Główna pętla programu
    while (1) {
        FD_ZERO(&readfds);    // Wyczyszczenie zestawu deskryptorów
        FD_SET(client_socket, &readfds);  // Dodanie socketu do monitorowania

        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // Timeout 100ms

        // Oczekiwanie na aktywność na gnieździe
        int activity = select(client_socket + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0) {
            printf("Select error");
            break;
        }

        // Sprawdzenie czy nadszedł czas na wysłanie pinga
        struct timeval current_time;
        gettimeofday(&current_time, NULL);

        // Obliczenie różnicy czasu w milisekundach
        long time_diff = (current_time.tv_sec - last_ping_time.tv_sec) * 1000 +
                        (current_time.tv_usec - last_ping_time.tv_usec) / 1000;

        if (time_diff >= current_ping_interval) {
            send_pings(client_socket);
            last_ping_time = current_time;
            current_ping_interval = get_random_ping_interval();
        }

        // Wysłanie zapytań kontrolnych
        send_keep_alive_check(client_socket);

        // Obsługa przychodzących danych
        if (FD_ISSET(client_socket, &readfds)) {
            client_listen(client_socket, server_addr, server_len);
        }
    }

    close(client_socket);  // Zamknięcie gniazda
    return 0;
}

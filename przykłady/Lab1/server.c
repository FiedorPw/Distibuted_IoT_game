#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>     // Biblioteka zawierająca funkcje do obsługi socketów
#include <arpa/inet.h>      // Biblioteka dla operacji internetowych (inet_pton, htons)
#include <unistd.h>         // Dla funkcji close()
#include <time.h>           // Dla funkcji time() używanej w generowaniu liczb losowych

#define SERVER_PORT 1307    // Port nasłuchiwania serwera
#define CLIENT_PORT 1305    // Port na który jest wysyłane do klienta
#define BUFFER_SIZE 1024    // Rozmiar bufora na wiadomości
#define CLIENT_IP "127.0.0.1"

// Definicje nagłówków komunikatów
#define HELLO 'h'    // Nagłówek wiadomości identyfikacyjnej serwera
#define PING 'i'     // Nagłówek żądania ping
#define PONG 'o'     // Nagłówek odpowiedzi na ping
#define REQUEST 'q'  // Nagłówek sprawdzenia aktywności
#define RESPONSE 's' // Nagłówek potwierdzenia aktywności

// Zmienna do pamiętania czy zainicjalizowało się RNG (generator liczb losowych)
static int seeded = 0;
// ID serwera które jest podawane jako parametr przy wywołaniu programu
static int SERVER_ID;

// Funkcja generująca losową liczbę z zakresu 0-9
int get_random_number() {
    if (!seeded) {
        // Inicjalizacja generatora przy pierwszym użyciu
        srand(time(NULL));
        seeded = 1;
    }
    return rand() % 10;
}

// Funkcja wyświetlająca informacje o nagłówkach protokołu
void print_protocol_headers() {
    printf("\nNagłówki Protokołu:\n");
    printf("================\n");
    printf("[%c] HELLO    - Wiadomość identyfikacyjna serwera\n", HELLO);
    printf("[%c] PING     - Żądanie ping od klienta\n", PING);
    printf("[%c] PONG     - Odpowiedź serwera na ping\n", PONG);
    printf("[%c] REQUEST  - Sprawdzenie aktywności od klienta\n", REQUEST);
    printf("[%c] RESPONSE - Potwierdzenie aktywności serwera\n", RESPONSE);
    printf("================\n\n");
}

// Funkcja przetwarzająca nagłówek wiadomości
// Usuwa nagłówek z wiadomości i zwraca samą treść
char* process_header(char* message) {
    if (message == NULL) {
        return NULL;
    }

    char header = message[0];

    // Wyświetlanie odpowiedniego komunikatu na podstawie nagłówka
    switch(header) {
        case HELLO:
            printf("\033[32mOtrzymano wiadomość HELLO\033[0m\n");
            break;
        case PING:
            printf("\033[32mOtrzymano wiadomość PING\033[0m\n");
            break;
        case PONG:
            printf("\033[32mOtrzymano wiadomość PONG\033[0m\n");
            break;
        case REQUEST:
            printf("\033[32mOtrzymano wiadomość REQUEST\033[0m\n");
            break;
        case RESPONSE:
            printf("\033[32mOtrzymano wiadomość RESPONSE\033[0m\n");
            break;
        default:
            printf("\033[31mNieznany nagłówek: %c\033[0m\n", header);
            return NULL;
    }

    // Utworzenie nowego ciągu znaków bez nagłówka
    // size_t to typ całkowity bez znaku przeznaczony do reprezentowania rozmiarów
    // używany w funkcjach zarządzania pamięcią i tablicami
    size_t msg_len = strlen(message) - 1;
    // malloc alokuje pamięć
    char* new_message = (char*)malloc(msg_len + 1);
    if (new_message == NULL) {
        return NULL;
    }

    strcpy(new_message, message + 1);  // Kopiowanie wiadomości bez nagłówka

    return new_message;
}

// Funkcja dodająca nagłówek do wiadomości
char* add_header(char* message, char header) {
    if (message == NULL) {
        return NULL;
    }

    // Obliczenie długości nowego ciągu znaków
    size_t msg_len = strlen(message);
    size_t new_len = msg_len + 2;  // +1 na nagłówek, +1 na terminator null

    // Alokacja pamięci na nowy ciąg znaków
    char* new_message = (char*)malloc(new_len);
    if (new_message == NULL) {
        return NULL;
    }

    // Dodanie nagłówka i skopiowanie wiadomości
    new_message[0] = header;
    strcpy(new_message + 1, message);

    return new_message;
}

// Funkcja inicjalizująca adres klienta
struct sockaddr_in init_client_adress(){
    // struct sockaddr_in - Struktura przechowująca informacje o adresie IPv4:
    // - sin_family: rodzina protokołów (AF_INET dla IPv4)
    // - sin_port: numer portu w kolejności sieciowej (big-endian)
    // - sin_addr: adres IP
    struct sockaddr_in client_addr;

    // memset - Wypełnia strukturę zerami aby uniknąć śmieci w pamięci
    // &client_addr - adres struktury
    // 0 - wartość wypełniająca
    // sizeof - rozmiar struktury w bajtach
    memset(&client_addr, 0, sizeof(client_addr));

    // Konfiguracja protokołu IPv4
    client_addr.sin_family = AF_INET;

    // htons - Konwersja portu na format sieciowy (host to network short)
    client_addr.sin_port = htons(CLIENT_PORT);

    // inet_pton - Konwersja adresu IP z formatu tekstowego na binarny
    // AF_INET - rodzina adresów IPv4
    // CLIENT_IP - adres w formie tekstowej
    // sin_addr - pole struktury na adres binarny
    if (inet_pton(AF_INET, CLIENT_IP, &client_addr.sin_addr) <= 0) {
        printf("Nieprawidłowy adres IP klienta\n");
        exit(1);
    }

    return client_addr;
}

// Funkcja wysyłająca wiadomość HELLO. Wysyła ID które jest zapisywane w kliencie.
void server_hello(int server_socket, struct sockaddr_in client_addr, socklen_t client_len) {
    char message[10];
    sprintf(message, "%d", SERVER_ID);
    char* message_with_header = add_header(message, HELLO);

    printf("\033[34mWysyłanie wiadomości: [%c]%s\033[0m\n",
           HELLO, message);

    if (message_with_header != NULL) {
        // Socket UDP - wysyłanie danych:
        // server_socket - deskryptor gniazda przez które wysyłamy
        // message_with_header - wskaźnik na dane do wysłania (nagłówek+wiadomość)
        // strlen() - długość wysyłanej wiadomości w bajtach
        // 0 - flagi (brak dodatkowych opcji)
        // (struct sockaddr*) - rzutowanie adresu na ogólną strukturę sockaddr
        // sizeof - rozmiar struktury z adresem odbiorcy w bajtach
        sendto(server_socket,
               message_with_header,
               strlen(message_with_header),
               0,
               (struct sockaddr*)&client_addr,
               sizeof(client_addr));
    }
    free(message_with_header);
}

// Funkcja obsługująca odpowiedź na PING
void ping_response(int server_socket, struct sockaddr_in client_addr, socklen_t client_len){
    char buffer[BUFFER_SIZE];

    // odebranie przez socket pinga
    int recv_len = recvfrom(server_socket,
                           buffer,
                           BUFFER_SIZE,
                           0,
                           (struct sockaddr*)&client_addr,
                           &client_len);

    if (recv_len > 0) {
        char* recived_message = process_header(buffer);

        buffer[recv_len] = '\0';
        printf("Otrzymano: %s\n", buffer);

        char temp_buffer[recv_len+2];
        int random_number = get_random_number();
        // konwersja liczby na znak ASCII przez dodanie do kodu znaku '0'
        temp_buffer[0] = '0'+ random_number;
        strcpy(temp_buffer + 1, buffer);

        char* new_buffer = add_header(temp_buffer, PONG);
        if (new_buffer == NULL) {
            printf("Nie udało się dodać nagłówka\n");
            return;
        }

        printf("\033[34mWysyłanie: %s \033[0m\n", new_buffer);
        sendto(server_socket,
              new_buffer,
              strlen(new_buffer),
              0,
              (struct sockaddr*)&client_addr,
              client_len);

        free(new_buffer);
    }
}

// Główna funkcja obsługująca przychodzące wiadomości
void handle_message(int server_socket, struct sockaddr_in client_addr, socklen_t client_len) {
    char buffer[BUFFER_SIZE];

    int recv_len = recvfrom(server_socket,
                           buffer,
                           BUFFER_SIZE,
                           0,
                           (struct sockaddr*)&client_addr,
                           &client_len);

    if (recv_len > 0) {
        // Dodanie terminatora null po ostatnim znaku odebranej wiadomości
        buffer[recv_len] = '\0';
        // wstawienie headera na pierwszy znak
        char header = buffer[0];

        printf("\033[35mOtrzymano wiadomość: [%c]%s\033[0m\n",
               header, buffer + 1);

        switch(header) {
            case PING: {
                printf("\033[32mOtrzymano PING\033[0m\n");
                char temp_buffer[recv_len + 2];
                int random_number = get_random_number();
                temp_buffer[0] = '0' + random_number;
                strcpy(temp_buffer + 1, buffer + 1);

                char* pong_msg = add_header(temp_buffer, PONG);
                if (pong_msg != NULL) {
                    printf("\033[34mWysyłanie PONG: [%c]%s\033[0m\n", PONG, temp_buffer);
                    sendto(server_socket,
                           pong_msg,
                           strlen(pong_msg),
                           0,
                           (struct sockaddr*)&client_addr,
                           client_len);
                    free(pong_msg);
                }
                break;
            }

            case REQUEST: {
                printf("\033[32mOtrzymano żądanie sprawdzenia aktywności\033[0m\n");
                char response = RESPONSE;
                printf("\033[34mWysyłanie odpowiedzi: [%c]\033[0m\n", response);
                sendto(server_socket,
                       &response,
                       1,
                       0,
                       (struct sockaddr*)&client_addr,
                       client_len);
                break;
            }

            default: {
                printf("\033[31mNieznany typ wiadomości: %c\033[0m\n", header);
                break;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Użycie: %s <port> <id_serwera>\n", argv[0]);
        printf("Przykład: %s 1306 1337\n", argv[0]);
        return 1;
    }

    int server_port = atoi(argv[1]);
    SERVER_ID = atoi(argv[2]);

    printf("Uruchamianie serwera na porcie %d z ID %d\n", server_port, SERVER_ID);
    print_protocol_headers();

    int server_socket;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t client_len = sizeof(client_addr);

    // Utworzenie gniazda UDP
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);

    // Konfiguracja adresu serwera
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_socket,
            (struct sockaddr*)&server_addr,
            sizeof(server_addr)) < 0) {
        perror("Błąd bind");
        exit(1);
    }

    client_addr = init_client_adress();

    printf("Serwer uruchomiony. Wysyłanie początkowej wiadomości HELLO...\n");
    sleep(1);
    server_hello(server_socket, client_addr, client_len);

    // Zmienne dla select()
    // Zmienne dla multipleksowania we/wy:
    // readfds - zbiór deskryptorów do monitorowania odczytu
    // tv - struktura określająca timeout dla select()
    // last_hello_time - czas ostatniej wysłanej wiadomości HELLO
    fd_set readfds;
    struct timeval tv;
    time_t last_hello_time = time(NULL);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);

        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms timeout

        int activity = select(server_socket + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0) {
            printf("Błąd select");
            break;
        }

        // Wysyłanie okresowych wiadomości HELLO
        time_t current_time = time(NULL);
        if (current_time - last_hello_time >= 5) {
            server_hello(server_socket, client_addr, client_len);
            last_hello_time = current_time;
        }

        // Obsługa przychodzących danych
        if (FD_ISSET(server_socket, &readfds)) {
            handle_message(server_socket, client_addr, client_len);
        }
    }

    close(server_socket);
    return 0;
}

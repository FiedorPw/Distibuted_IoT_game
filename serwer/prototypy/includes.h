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

#include <WiFi.h>
#include <WiFiUdp.h>

// Dane logowania do sieci Wi-Fi
const char* ssid = "GreenNet1";
const char* password = "niunia1234";

// Chmurowy adres serwera
IPAddress serverIp(34,0,240,164); // Adres IP serwera/
unsigned int localPort = 1305;
unsigned int serverPort = 1337;

// Ustawienia sieciowe
//unsigned int localPort = 1305;
//unsigned int serverPort = 1305;
//IPAddress serverIp(192, 168, 0, 208); // Adres IP serwera

// Ustawienia protokołu
const int sequenceLength = 4;
String userSequence = "";
String serverSequence = "";
const int buttonPin = 0;
unsigned long buttonPressTime = 0;
bool buttonState = false;
bool lastButtonState = false;
bool sequenceEntered = false;
bool isWinner = false;
bool gameStarted = false;

// Bufor na odbierane dane
char packetBuffer[255];

// Obiekt UDP
WiFiUDP Udp;

// Nagłówki protokołu komunikacyjnego
#define START 'S'
#define WINNER 'W'
#define REGISTER 'R'
#define GAME 'G'
#define ACK 'a'

// Id hosta
int hostId = 1;

void setup() {
  // Inicjalizacja komunikacji szeregowej
  Serial.begin(115200);

  // Łączenie z siecią Wi-Fi
  Serial.print("Łączenie z ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Połączono z Wi-Fi");
  Serial.print("Adres IP: ");
  Serial.println(WiFi.localIP());

  // Start UDP
  Udp.begin(localPort);

  pinMode(buttonPin, INPUT_PULLUP);

  // Proces ustawiania sekwencji
  Serial.println("Rozpocznij wpisywanie sekwencji za pomocą przycisku BOOT:");
  while (userSequence.length() < sequenceLength) {
    buttonState = digitalRead(buttonPin) == LOW;
    if (buttonState && !lastButtonState) {
      buttonPressTime = millis();
    }
    if (!buttonState && lastButtonState) {
      unsigned long pressDuration = millis() - buttonPressTime;
      if (pressDuration < 500) {
        userSequence += "0";
        Serial.println("Dodano: 0");
      } else {
        userSequence += "1";
        Serial.println("Dodano: 1");
      }
      signalInput();
    }
    lastButtonState = buttonState;
  }

  // Po zakończeniu wprowadzania sekwencji
  if (userSequence.length() == sequenceLength) {
    Serial.print("Wprowadzona sekwencja: ");
    Serial.println(userSequence);
    Serial.println("Sekwencja została ustawiona.");
    sequenceEntered = true;

    // Wysyłanie informacji rejestracyjnej do serwera po wprowadzeniu sekwencji
    sendRegistrationInfo();
  }
}

void loop() {
  // Odbieranie pakietów
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    Serial.print("Odebrano pakiet o rozmiarze ");
    Serial.println(packetSize);
    Serial.print("Od ");
    IPAddress remote = Udp.remoteIP();
    for (int i = 0; i < 4; i++) {
      Serial.print(remote[i], DEC);
      if (i < 3) {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.println(Udp.remotePort());

    // Odczytanie danych z pakietu
    Udp.read(packetBuffer, 255);
    packetBuffer[packetSize] = '\0';

    // Przetwarzanie pakietu
    processPacket(packetBuffer, remote, Udp.remotePort());
  }

      // Sprawdzenie, czy gra się rozpoczęła i czy sekwencja została wprowadzona
  if (sequenceEntered && gameStarted) {
      
      if (serverSequence.length() >= sequenceLength) {
          String lastServerChars = serverSequence.substring(serverSequence.length() - sequenceLength);
          if (lastServerChars.equals(userSequence)) {
              Serial.println("Sekwencja serwera odpowiada sekwencji gracza!");
              
              // Wysyłanie oddzielnych wiadomości ACK i WINNER
              //sendACK(serverIp, serverPort, GAME); // Wysyłanie ACK dla ostatniej odebranej wiadomości GAME
              delay(100); // Dodanie krótkiego opóźnienia
              sendWinnerMessage(serverIp, serverPort, '1');
              
              isWinner = true;
              gameStarted = false;
              serverSequence = ""; // Wyczyść sekwencję serwera tylko w przypadku wygranej
          }
          // Usunięto serverSequence = ""; w przypadku, gdy sekwencje się nie zgadzają
      }
  
  }

  delay(50);
}

void processPacket(char *packet, IPAddress remoteIP, unsigned int remotePort) {
  char header = packet[0];
  char *message = packet + 1;

  Serial.print("Odebrano nagłówek: ");
  Serial.println(header);

  switch (header) {
    case START:
        Serial.println("Odebrano START");
        gameStarted = true;
        sendACK(remoteIP, remotePort, START);
        break;
    case WINNER:
        Serial.println("Odebrano WINNER");
        if (message[0] == '0') {
          Serial.println("Przegrana, resetowanie stanu gry");
          serverSequence = "";
          gameStarted = false;
        }
        break;
    case REGISTER:
        Serial.println("Odebrano REGISTER");
        sendACK(remoteIP, remotePort, REGISTER);
        break;
    case GAME:
        Serial.println("Odebrano GAME");
        if (gameStarted) {
          if (strlen(message) > 0) {
            serverSequence += message[0];
          }
          Serial.print("Aktualna sekwencja serwera: ");
          Serial.println(serverSequence);
          sendACK(remoteIP, remotePort, GAME);
        } else {
          Serial.println("Odebrano GAME przed START, ignorowanie.");
        }
        break;
    case ACK:
        Serial.print("Odebrano ACK dla: ");
        Serial.println(message);
        break;
    default:
        Serial.println("Nieznany nagłówek");
  }
}

void sendACK(IPAddress remoteIP, unsigned int remotePort, char header) {
  char ackMessage[3] = {ACK, header, '\0'};
  Udp.beginPacket(remoteIP, remotePort);
  Udp.write((const uint8_t *)ackMessage, 2);
  Udp.endPacket();
  Serial.print("Wysłano ACK dla: ");
  Serial.println(header);
}

void sendRegistrationInfo() {
  String message = WiFi.localIP().toString() + "," + localPort + "," + hostId;
  char messageBuffer[50];
  message.toCharArray(messageBuffer, sizeof(messageBuffer));

  Udp.beginPacket(serverIp, serverPort);
  Udp.write(REGISTER);
  Udp.write((const uint8_t *)messageBuffer, strlen(messageBuffer));
  Udp.endPacket();

  Serial.print("Wysłano informacje rejestracyjne do serwera: ");
  Serial.println(message);
}

void signalInput() {
  int ledPin = 2; 
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  delay(100);
  digitalWrite(ledPin, LOW);
  delay(100);
}

void sendWinnerMessage(IPAddress remoteIP, unsigned int remotePort, char result) {
  char winnerMessage[] = {WINNER, result, 'X', '\0'};
  Udp.beginPacket(remoteIP, remotePort);
  Udp.write((const uint8_t *)winnerMessage, 3);
  Udp.endPacket();
  Serial.print("Wysłano WINNER ");
  Serial.println(result);
}
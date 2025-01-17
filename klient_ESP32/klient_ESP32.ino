#include <WiFi.h>
#include <WiFiUdp.h>

// Chmurowy adres serwera
IPAddress serverIp(34,0,240,164); // Adres IP serwera/
unsigned int localPort = 1337;
unsigned int serverPort = 1337;

// Dane logowania do sieci Wi-Fi
const char* ssid = "2.4GHz_CyberNet";
const char* password = "ortogonalnosc";

// Ustawienia protokołu
#define SEQUENCE_LENGTH 4
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
#define BUFFER_SIZE 255
char packetBuffer[BUFFER_SIZE];

// Obiekt UDP
WiFiUDP Udp;

// Nagłówki protokołu komunikacyjnego
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

// Id hosta
int hostId = 1;

// Inicjalizacja builin diody
int ledPin = 2;

void setup() {
  // Inicjalizacja komunikacji szeregowej
  Serial.begin(115200);

  // Inicjalizacja builin diody 
  pinMode(ledPin, OUTPUT);

  // Łączenie z siecią Wi-Fi
  Serial.print("\n\nŁączenie z ");
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
  while (userSequence.length() < SEQUENCE_LENGTH) {
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
      flash_diode();
    }
    lastButtonState = buttonState;
  }

  // Po zakończeniu wprowadzania sekwencji
  if (userSequence.length() == SEQUENCE_LENGTH) {
    Serial.print("\nWprowadzona sekwencja: ");
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
    Serial.print("\nOdebrano pakiet o rozmiarze ");
    Serial.print(packetSize);
    Serial.print(" Od ");
    IPAddress remote = Udp.remoteIP();
    for (int i = 0; i < 4; i++) {
      Serial.print(remote[i], DEC);
      if (i < 3) {
        Serial.print(".");
      }
    }
    Serial.print(":");
    Serial.println(Udp.remotePort());

    // Odczytanie danych z pakietu
    int len = Udp.read(packetBuffer, BUFFER_SIZE);
     if (len > 0) {
        packetBuffer[len] = '\0'; // Upewnij się, że bufor jest zakończony znakiem null
    }
    

    // Przetwarzanie pakietu
    processPacket(packetBuffer, remote, Udp.remotePort());
  }

    // Sprawdzenie, czy gra się rozpoczęła i czy sekwencja została wprowadzona
  if (sequenceEntered && gameStarted) {
    //Serial.print("Aktualna sekwencja serwera przed sprawdzeniem: "); // Dodaj tę linię
        if (serverSequence.length() >= SEQUENCE_LENGTH) {
            String lastServerChars = serverSequence.substring(serverSequence.length() - SEQUENCE_LENGTH);
            if (lastServerChars.equals(userSequence)) {
                Serial.println("\n\nSekwencja serwera odpowiada sekwencji gracza - wygrałeś!!!");
                flash_diode_multiple(3);
                
                // Wysyłanie wiadomości WINNER
                sendWinnerMessage(serverIp, serverPort, WIN_RESULT);
                
                isWinner = true;
                gameStarted = false;
                serverSequence = ""; // Wyczyść sekwencję serwera tylko w przypadku wygranej
            }
        }

  }

  delay(50);
}

void processPacket(char *packet, IPAddress remoteIP, unsigned int remotePort) {
    if (packet == NULL) {
        Serial.println("Odebrano pusty pakiet.");
        return;
    }

    uint8_t header = packet[0];
    Serial.print("Odebrano nagłówek: 0x");
    Serial.print(header, HEX);
    Serial.print(", Dane: 0x");  
    Serial.print(packet[1], HEX); 


    switch (header) {
        case START_HEADER:
            Serial.println(" - START");
            gameStarted = true;
            sendACK(remoteIP, remotePort, START_HEADER);
            break;
        case WINNER_HEADER:
            Serial.print(" - WINNER, ");
            if (packet[1] == LOSE_RESULT) {
            Serial.println(" resetowanie stanu gry");
                serverSequence = "";
                gameStarted = false;
            }
            break;
        case REGISTER_HEADER:
            Serial.println("Odebrano REGISTER - ignorowanie");
            sendACK(remoteIP, remotePort, REGISTER_HEADER);
            break;
        case GAME_HEADER:
            Serial.println(" - GAME(update)");
            if (gameStarted) {
                char result = (packet[1] == TAILS) ? '1' : '0';
                serverSequence += result; // Poprawiona linijka - usunięto niepotrzebny warunek
                Serial.print("Aktualna sekwencja serwera: ");
                Serial.println(serverSequence);
                sendACK(remoteIP, remotePort, GAME_HEADER);
            } else {
                Serial.println("Odebrano GAME przed START, ignorowanie.");
            }
            break;
                case ACK_SERVER_HEADER:
                    Serial.print("\nOdebrano ACK dla: ");
                    break;
        default:
            Serial.println("Nieznany nagłówek");
    }
}

void sendACK(IPAddress remoteIP, unsigned int remotePort, uint8_t header) {
  uint8_t buffer[2];
  buffer[0] = ACK_CLIENT_HEADER;
  buffer[1] = header; // Drugi bajt to numer nagłówka potwierdzanej wiadomości

  Udp.beginPacket(remoteIP, remotePort);
  Udp.write(buffer, sizeof(buffer));
  Udp.endPacket();
  Serial.print("Wysłano ACK dla: 0x");
  Serial.println(header, HEX);
}

void sendRegistrationInfo() {
  uint8_t buffer[8];
  buffer[0] = REGISTER_HEADER;

  // ID klienta - zakładamy, że mieści się w zakresie 0-255
  buffer[1] = (uint8_t)hostId;

  // Adres IP klienta
  IPAddress localIP = WiFi.localIP();
  buffer[2] = localIP[0];
  buffer[3] = localIP[1];
  buffer[4] = localIP[2];
  buffer[5] = localIP[3];

  // Port klienta (w formacie sieciowym - big-endian)
  uint16_t localPortNetworkOrder = htons(localPort);
  memcpy(&buffer[6], &localPortNetworkOrder, 2);

  Udp.beginPacket(serverIp, serverPort);
  Udp.write(buffer, sizeof(buffer));
  Udp.endPacket();

  Serial.print("Wysłano prośbę o zarejestrowanie do serwera: ");
  for (size_t i = 0; i < sizeof(buffer); i++) {
      Serial.print("0x");
      Serial.print(buffer[i], HEX); // Print in hexb
      Serial.print(" ");
  }
  Serial.println();
}

void flash_diode() {
  digitalWrite(ledPin, HIGH);
  delay(100);
  digitalWrite(ledPin, LOW);
  delay(100);
}

void flash_diode_multiple(int times_flash) {
  for (int i = 0; i < times_flash; i++) {
    flash_diode();
  }
}

void sendWinnerMessage(IPAddress remoteIP, unsigned int remotePort, uint8_t result) {
  uint8_t buffer[2];
  buffer[0] = WINNER_HEADER;
  buffer[1] = result;

  Udp.beginPacket(remoteIP, remotePort);
  Udp.write(buffer, sizeof(buffer));
  Udp.endPacket();
  Serial.print("Wysłano WINNER z kodem: 0x");
  Serial.println(result, HEX);
}

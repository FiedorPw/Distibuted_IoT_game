#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define SERVER_ID 1000
// headers
#define HELLO 'h'    // Nagłówek wiadomości identyfikacyjnej serwera
#define PING 'i'     // Nagłówek żądania ping(nieobsługiwane)
#define PONG 'o'     // Nagłówek odpowiedzi na ping(nieobsługiwane)
#define REQUEST 'q'  // Nagłówek sprawdzenia aktywności
#define RESPONSE 's' // Nagłówek potwierdzenia aktywności

//wifi 
const char* ssid = "KID";       // Your WiFi SSID
const char* password = "!234Qwer"; // Your WiFi password
//UDP
WiFiUDP udp;
const unsigned int udpPort = 1305;    // Port to listen on
char incomingPacket[255];            // buffer for packets

// Komunikacja z serwerem
static const IPAddress clientIP(192, 168, 1, 104);
static const unsigned int clientPort = 1305; 
unsigned long previousServerHello = 0;
const unsigned long serverHelloInterval = 2000; // 100ms

//led consts
const int ledPin = LED_BUILTIN; // Built-in LED (GPIO 2)
unsigned long previousMillis = 0;
unsigned long startMillis = 0;
const unsigned long blinkInterval = 70; // 70ms
const unsigned long blinkDuration = 975; // 975ms
bool ledState = LOW;
unsigned long blinkingStart = 0;
bool isBlinking = false;
unsigned long currentMillis = 0;


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

void server_hello(IPAddress clientIP, unsigned int clientPort) {
    char message[10];
    sprintf(message, "%d", SERVER_ID);
    char* message_with_header = add_header(message, HELLO);

    printf("\033[34mWysyłanie wiadomości: [%c]%s\033[0m\n",
           HELLO, message);

    if (message_with_header != NULL) {
        udp.beginPacket(clientIP,clientPort);
        udp.write(message_with_header);
        udp.endPacket();
    }
    free(message_with_header);
}

char* process_header(char* message) {
    if (message == NULL) {
        return NULL;
    }

    char header = message[0];

    // używany w funkcjach zarządzania pamięcią i tablicami
    size_t msg_len = strlen(message) - 1;
    // malloc alokuje pamięć
    char* new_message = (char*)malloc(msg_len + 1);
    if (new_message == NULL) {
        return NULL;
    }

    strcpy(new_message, message + 1);  // Kopiowanie wiadomości bez nagłówka


    // Wyświetlanie odpowiedniego komunikatu na podstawie nagłówka
    switch(header) {
        case HELLO:
            printf("\033[32mOtrzymano wiadomość HELLO (NIEOBSŁUGIWANY)\033[0m\n");
            break;
        case PING:
            printf("\033[32mOtrzymano wiadomość PING (NIEOBSŁUGIWANY)\033[0m\n");
            break;
        case PONG:
            printf("\033[32mOtrzymano wiadomość PONG (NIEOBSŁUGIWANY)\033[0m\n");
            break;
        case REQUEST:
            printf("\033[32mOtrzymano wiadomość REQUEST\033[0m\n");
            udp.beginPacket(clientIP,clientPort);
            printf("\033[32mWysłano RESPONSE\033[0m\n");
            udp.write("s");
            udp.endPacket(); 
            break;
        case RESPONSE:
            printf("\033[32mOtrzymano wiadomość RESPONSE (NIEOBSŁUGIWANY) \033[0m\n");
            break;
        default:
            printf("\033[31mNieznany nagłówek: %c\033[0m\n", header);
            return NULL;
    }

   
    return new_message;
}

void fast_blink(long currentMillis){

  // Start blinking logic
  if (!isBlinking) {
    isBlinking = true;
    startMillis = currentMillis;
  }

  // Check if blinking duration is over
  if (isBlinking && (currentMillis - startMillis >= blinkDuration)) {
    isBlinking = false;
    digitalWrite(ledPin, HIGH); // Ensure the LED is off after blinking
  }

  // Handle the blink
  if (isBlinking) {
    if (currentMillis - previousMillis >= blinkInterval) {
      previousMillis = currentMillis;
      ledState = !ledState; //zmień stan 
      digitalWrite(ledPin, ledState ? LOW : HIGH); // Inverted logic
    }
  }

}

void setup() {
  Serial.begin(115200);              
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH); // Turn LED off (inverted logic on ESP8266)

  WiFi.begin(ssid, password);  // connecting to wifi
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());    // Print ESP8266 IP address
  
  // Start UDP
  udp.begin(udpPort);
  Serial.print("Listening on UDP port: ");
  Serial.println(udpPort);
  // pierwsze wysłanie hello
  server_hello(clientIP, clientPort);
}

void loop() {
    // blinking
    currentMillis = millis();


    // blink if 975ms of blinking haven't passed since triggered by packet
    if(currentMillis - blinkingStart <= blinkDuration){
        fast_blink(currentMillis);
    }

    if(currentMillis - previousServerHello >= serverHelloInterval){
        server_hello(clientIP, clientPort);
        previousServerHello = currentMillis;
    }

    //reciving packets
    int packetSize = udp.parsePacket();
    if (packetSize) {
        // announce packet by blinking- 
        // starting timer which counts down time of blinking
        blinkingStart = currentMillis;
    
        // debug
        // Serial.print("Received packet of size ");
        // Serial.println(packetSize);
        // Serial.print("From IP: ");
        // Serial.println(udp.remoteIP());
        // Serial.print("From port: ");
        // Serial.println(udp.remotePort());

        // Read the packet into the buffer
        int len = udp.read(incomingPacket, 255);
        if (len > 0) {
        incomingPacket[len] = '\0'; // Null-terminate the string
        }
        Serial.print("Received: ");
        Serial.println(incomingPacket);
        process_header(incomingPacket);

        // // Optionally send a response
        // udp.beginPacket(udp.remoteIP(), udp.remotePort());
        // udp.print("Message received!\n");
        // udp.endPacket();
    
  }

    
}

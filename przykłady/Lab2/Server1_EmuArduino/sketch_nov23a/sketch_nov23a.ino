#include <ZsutDhcp.h>           // For obtaining IP from DHCP (for ebsim)
#include <ZsutEthernet.h>       // Required for 'ZsutEthernetUDP' class
#include <ZsutEthernetUdp.h>    // The 'ZsutEthernetUDP' class itself
#include <ZsutFeatures.h>       // For: ZsutMillis()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>             // For `close()`
#include <time.h>               // For `time()` used in random number generation

#define UDP_SERVER_PORT         1307  // Port number for server
#define CLIENT_IP "192.168.1.104"   // Client IP
#define CLIENT_PORT 1305            // Client port
#define SERVER_ID 1337              // Unique server ID

#define HELLO 'h'    // Server identification message header
#define PING 'i'     // Ping request header
#define PONG 'o'     // Ping response header
#define REQUEST 'q'  // Activity check request header
#define RESPONSE 's' // Activity check confirmation header

#define PACKET_BUFFER_LENGTH        512
unsigned char packetBuffer[PACKET_BUFFER_LENGTH];

// MAC address for Ethernet card
byte MAC[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x01}; 

unsigned int localPort = UDP_SERVER_PORT;    
uint16_t t;

// Use a single instance of ZsutEthernetUDP
ZsutEthernetUDP Udp;            

// Adds a header to the message
char* add_header(char* message, char header) {
    if (message == NULL) {
        return NULL;
    }
    size_t msg_len = strlen(message);
    size_t new_len = msg_len + 2;  // +1 for header, +1 for null terminator
    char* new_message = (char*)malloc(new_len);
    if (new_message == NULL) {
        return NULL;
    }
    new_message[0] = header;
    strcpy(new_message + 1, message);
    return new_message;
}

// Process incoming message header
void process_header(char* message) {
    if (message == NULL) {
        return;
    }
    
    char header = message[0];
    switch(header) {
        case HELLO:
            Serial.println("Received HELLO message");
            break;
        case PING:
            Serial.println("Received PING message");
            break;
        case PONG:
            Serial.println("Received PONG message");
            break;
        case REQUEST:
            Serial.println("Received REQUEST message");
            break;
        case RESPONSE:
            Serial.println("Received RESPONSE message");
            break;
        default:
            Serial.println("Unknown message header");
            break;
    }
}

// Sends HELLO to the client
void server_hello(ZsutIPAddress clientIP, uint16_t clientPort) {
    char message[10];
    sprintf(message, "%d", SERVER_ID);  // Create a message with the server ID

    char* message_with_header = add_header(message, HELLO);

    if (message_with_header != NULL) {
        Serial.print("Sending HELLO message: [");
        Serial.print(HELLO);
        Serial.print("] ");
        Serial.println(message);

        Udp.beginPacket(clientIP, clientPort);                // Start UDP packet
        Udp.write((uint8_t*)message_with_header, strlen(message_with_header));  // Write data
        Udp.endPacket();                                      // Send the packet
        free(message_with_header);                           // Free allocated memory
    } else {
        Serial.println("Failed to allocate memory for HELLO message.");
    }
}

// Function to read and display raw analog values
int read_temperature() {
    int raw_value = ZsutAnalog5Read();  // Read the raw analog value from Z3
    Serial.print("Raw Analog Value (0-1023): ");
    Serial.println(raw_value);         // Print the raw value
    return raw_value;
}

// Function to handle ping response
void handle_ping_response(ZsutIPAddress clientIP, uint16_t clientPort, char* received_message) {
    int random_number = rand() % 10;  // Generate a random number between 0-9
    char response[PACKET_BUFFER_LENGTH];
    snprintf(response, PACKET_BUFFER_LENGTH, "%d%s", random_number, received_message + 1);  // Append random number to the message

    char* pong_message = add_header(response, PONG);
    if (pong_message != NULL) {
        Serial.print("Sending PONG message: ");
        Serial.println(pong_message);

        Udp.beginPacket(clientIP, clientPort);
        Udp.write((uint8_t*)pong_message, strlen(pong_message));
        Udp.endPacket();
        free(pong_message);
    } else {
        Serial.println("Failed to allocate memory for PONG message.");
    }
}

// Initialization
void setup() {
    Serial.begin(115200);
    Serial.print(F("Zsut eth udp server init... ["));
    Serial.print(F(__FILE__));
    Serial.print(F(", "));
    Serial.print(F(__DATE__));
    Serial.print(F(", "));
    Serial.print(F(__TIME__));
    Serial.println(F("]"));

    ZsutEthernet.begin(MAC);  // Initialize Ethernet card

    Serial.print(F("My IP address: "));
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
        Serial.print(ZsutEthernet.localIP()[thisByte], DEC);
        if (thisByte < 3) Serial.print(F("."));
    }
    Serial.println();

    Udp.begin(localPort);  // Start listening on UDP port
}

void loop() {
    // Read and display temperature periodically
    read_temperature();

    // Check for incoming packets
    int packetSize = Udp.parsePacket();  // Check for incoming packet
    if (packetSize > 0) {
        int len = Udp.read(packetBuffer, PACKET_BUFFER_LENGTH - 1);  // Read the packet
        if (len > 0) {
            packetBuffer[len] = '\0';  // Null-terminate the buffer
            Serial.print("Received: ");
            Serial.println((char*)packetBuffer);

            // Process the received message
            process_header((char*)packetBuffer);

            // Handle specific message types
            char header = packetBuffer[0];
            if (header == PING) {
                handle_ping_response(Udp.remoteIP(), Udp.remotePort(), (char*)packetBuffer);
            } else if (header == REQUEST) {
                // Read current temperature
                int temperature = read_temperature();

                // Format the response message
                char response[PACKET_BUFFER_LENGTH];
                snprintf(response, PACKET_BUFFER_LENGTH, "Temperature: %d", temperature);

                // Add RESPONSE header
                char* response_with_header = add_header(response, RESPONSE);
                if (response_with_header != NULL) {
                    // Send RESPONSE message
                    Serial.print("Sending RESPONSE message: ");
                    Serial.println(response_with_header);

                    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
                    Udp.write((uint8_t*)response_with_header, strlen(response_with_header));
                    Udp.endPacket();

                    free(response_with_header);
                } else {
                    Serial.println("Failed to allocate memory for RESPONSE message.");
                }
            }

            // Send a HELLO message periodically
            server_hello(Udp.remoteIP(), Udp.remotePort());
        }
    }
    delay(200); // Delay to reduce temperature reading frequency
}

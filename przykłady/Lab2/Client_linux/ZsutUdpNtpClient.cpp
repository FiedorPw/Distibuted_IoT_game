/*  Program demonstracyjny "Zsut NTP client", bazuje na UdpNtpClient dostarczanym z biblioteka "Arduino Ethernet"
 *
 *  Opis:
 *  Dziala w taki sposob, ze cyklicznie wysyla pakiet zadania NTP do servera NTP (time.coi.pw.edu.pl)
 *  a po otrzymaniu odpowiedzi przekształca otrzymane dane w aktualny czas UTC
 *  Problemy:
 *  -nie potrafi nawiazac polaczenia poprzez podanie adresu FQDN a tylko za pomoca adresu IP,
 *  -pakiet zadania NTP wypelniony nieco nieelegancko - poprzez ustawienie pol w tablicy,
 *  -odebrany pakiet odpowiedzi NTP jest traktowany dosc lekko - takeze jako tablica, bez
 *   sprawdzania poprawnosci odpowiedzi
 * 
 *  Autor:  Aleksander Pruszkowski 
 *  Data:   2020.04.04
 *
 *  Etykietka bazy: 
 Udp NTP Client
 Get the time from a Network Time Protocol (NTP) time server
 Demonstrates use of UDP sendPacket and ReceivePacket
 For more on NTP time servers and the messages needed to communicate with them,
 see http://en.wikipedia.org/wiki/Network_Time_Protocol
 created 4 Sep 2010
 by Michael Margolis
 modified 9 Apr 2012
 by Tom Igoe
 modified 02 Sept 2015
 by Arturo Guadalupi
 This code is in the public domain.
*/

#include <ZsutEthernet.h>
#include <ZsutEthernetUdp.h>
#include <ZsutFeatures.h>       //for: ZsutMillis()

//prototypy 
void ShowNtpResponse(byte *packetBuffer, int n);
void sendNTPpacket(ZsutIPAddress address_ip);

void sendNTPpacket(ZsutIPAddress address_ip);

byte mac[]={0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x01}; //MAC adres karty sieciowej, to powinno byc unikatowe - proforma dla ebsim'a 

#define UDP_SERVER_PORT     8888

unsigned int localPort=UDP_SERVER_PORT; // local port to listen for UDP packets - w tym przypadku malo istotne, ale trzeba ustawic

//adres serwera NTP skonwertowany z postaci FQDN: time.coi.pw.edu.pl
//do postaci IP
ZsutIPAddress timeServer_ip=ZsutIPAddress(194,29,130,252);
//Dla innego adresu serwera NTP mozna uzyskac za pomoca polecenia wydanego z CMD:
//  ping moj.ulubiony.serwer.ntp.pl
//lub gdy nie znamy serwera NTP mozemy zapytac czy konsorcjum NTP (ntp.org) poleca nam jakis:
//  ping pool.ntp.org
//pamietajac, iz odpowiedz dotyczyc bedzie byc moze losowego serwera NTP

const int NTP_PACKET_SIZE=48;       //Wielkosc danych zwiazanych z protokoken NTP: "NTP time stamp is in the first 48 bytes of the message"
byte packetBuffer[NTP_PACKET_SIZE]; //Bufore na dane NTP: "buffer to hold incoming and outgoing packets"

//Deklaracja interwalu co jaki czas program bedzie zadal od serwera NTP danych
#define NTP_CLIENT_PERIOD       (5000UL)   //5sek - prosze zwrocic uwage na "UL" na koncu - niezbedne gdy chcemy aby stala byla typu 'unsigned long'
unsigned long new_ntp_communications;
  
ZsutEthernetUDP Udp;

//Wersja funkcji delay() bazujaca na funkcjonalnosci dostepnej wylacznie w EBSimUnoEth
//W tej implementacji klienta NTP nie jest ona potrzebna
//void my_delay(unsigned long d){   //my_delay() obchodzi problem zlych zmiennych w implementacji liczenia czasu w EBSimUnoEth
//    unsigned long t1=ZsutMillis()+d;
//    for(;;)
//        if(t1<ZsutMillis())
//            break;
//}

#define NTP_MACHINE_STATE_IDLE        0         //stan bezczynnosci
#define NTP_MACHINE_STATE_SENT_REQ    1         //stan kiedy wlasnie wyslano zadanie i czekamy na odpowiedz
int ntp_comm_state=NTP_MACHINE_STATE_IDLE;	//nic nie wyslano

void setup() {
    //Zwyczajowe przywitanie z userem (niech wie ze system sie uruchomil poprawnie)
    Serial.begin(115200);
    Serial.println(F("Zsut eth ntp client init..."));

    //inicjaja karty sieciowej - proforma dla EBSimUnoEth'a
    ZsutEthernet.begin(mac);

    //Uruchomienie nasluchiwania na datagaramy UDP - dla klienta NTP to numer portu odbiorczego 
    Udp.begin(localPort);    

    new_ntp_communications=ZsutMillis();         //nowe zadanie od serwera NTP ma sie odbyc juz na poczatku dzialania programu
    ntp_comm_state=NTP_MACHINE_STATE_IDLE;      //zaczyna od stanu bezczynnosci
}

void loop(){
    //Poczatek nie blokujacego kodu cyklicznego odpytywania serwera NTP ---------->
    //czy maszyna stanow konwersacji z serverem NTP jest w stanie bezczynnosci?
    if(ntp_comm_state==NTP_MACHINE_STATE_IDLE){
        if(new_ntp_communications<ZsutMillis()){                             //is time for a new NTP server request?
            new_ntp_communications=ZsutMillis()+NTP_CLIENT_PERIOD;           //time when send a next new request
            Serial.println(F("NTP request sending..."));
            sendNTPpacket(timeServer_ip);                                   //send an NTP packet to a time server
            ntp_comm_state=NTP_MACHINE_STATE_SENT_REQ;
        }
    //jezeli maszyna stanow konwersacji z serverem NTP nie jest w stanie bezczynnosci
    //to moze wlasnie czekamy na odpowiedz od serewera?
    }else if(ntp_comm_state==NTP_MACHINE_STATE_SENT_REQ){
        if (Udp.parsePacket()){
            int n=Udp.read(packetBuffer, NTP_PACKET_SIZE);
            ShowNtpResponse(packetBuffer, n);
            ntp_comm_state=NTP_MACHINE_STATE_IDLE;
        }
        //wedlug dobrych praktyk tutaj powinnismy sprawdzic czy aby czas od poprzednio
        //wyslanego zadania nie przekroczyl zadanej wartosci (tzw. TIMEOUT)
        //wtedy powinnismy natychmiast przejsc do ponowienia wysylania zadania
        //...ale tego nie zakodowalem jeszcze ;->
    //stan w jakim nie powinnismy sie znalezc - jakis nie zamierzony blad (przeplenienie stosu, ...)?
    }else{  
        Serial.print(F("NTP machine unknown state: "));Serial.println(ntp_comm_state);
        ntp_comm_state=NTP_MACHINE_STATE_IDLE;         //profilaktycznie przelaczamy sie na stan bezczynnosci
    }
    //<----------Koniec nie blokujacego kodu cyklicznego odpytywania serwera NTP
    
    //Inne zadania do wykonania niemal rownolegle do powyzszego kodu
    ZsutEthernet.maintain();        //np.: zasadniczo odnawianie adresu pobranego z DHCP - proforma dla EBSimUnoEth'a
}

// send an NTP request to the NTP time server at the given address
void sendNTPpacket(ZsutIPAddress address_ip){
    memset(packetBuffer, 0, NTP_PACKET_SIZE);       // set all bytes in the buffer to 0
    //Malo eleganckie zainicjowanie wiadomosci do serwera NTP "Initialize values needed to form NTP request (see URL above for details on the packets)"
    packetBuffer[0] = 0b11100011;                   // LI, Version, Mode
    packetBuffer[1] = 0;                            // Stratum, or type of clock
    packetBuffer[2] = 6;                            // Polling Interval
    packetBuffer[3] = 0xEC;                         // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;

    // all NTP fields have been given values, now you can send a packet requesting a timestamp:
    Udp.beginPacket(address_ip, 123);               // NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}

void ShowNtpResponse(byte *packetBuffer, int n){
    if(n>=NTP_PACKET_SIZE){                                     //potrzebujemy m.in. pole o ofsecie 43 wiec odpowiedz nie moze byc krotsza niz NTP_PACKET_SIZE=48
        unsigned long highWord=word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord=word(packetBuffer[42], packetBuffer[43]);
        // combine the four bytes (two words) into a long integer this is NTP time (seconds since Jan 1 1900):
        unsigned long secsSince1900 = highWord << 16 | lowWord;
        Serial.print(F("Seconds since Jan 1 1900 = "));
        Serial.println(secsSince1900);
        Serial.print(F("Unix time = "));                        // now convert NTP time into everyday time:
        const unsigned long seventyYears=2208988800UL;          // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
        unsigned long epoch=secsSince1900-seventyYears;         // subtract seventy years:
        Serial.println(epoch);                                  // print Unix time:
        // print the hour, minute and second:
        Serial.print(F("The UTC time is "));                    // UTC is the time at Greenwich Meridian (GMT)
        Serial.print((epoch  % 86400L) / 3600);                 // print the hour (86400 equals secs per day)
        Serial.print(F(":"));
        if(((epoch % 3600)/60)<10){                             // In the first 10 minutes of each hour, we'll want a leading '0'
            Serial.print(F("0"));
        }
        Serial.print((epoch  % 3600)/60);                       // print the minute (3600 equals secs per minute)
        Serial.print(F(":"));
        if((epoch % 60)<10){                                    // In the first 10 seconds of each minute, we'll want a leading '0'
            Serial.print(F("0"));
        }
        Serial.println(epoch % 60);                             // print the second
    }else{
        Serial.println(F("NTP server respons is incorrect!"));
    }
}


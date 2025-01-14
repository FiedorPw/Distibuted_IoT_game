const int buttonPin = 0;  // Przyciski wbudowane na GPIO0 (BOOT)
const int sequenceLength = 4;  // Długość sekwencji
String userSequence = "";  // Przechowywana sekwencja

unsigned long buttonPressTime = 0;  // Czas naciśnięcia przycisku
bool buttonState = false;  // Stan przycisku
bool lastButtonState = false;  // Ostatni stan przycisku

void setup() {
  // Inicjalizacja przycisku BOOT
  pinMode(buttonPin, INPUT_PULLUP);  // Przycisk na GPIO0 (BOOT)
  
  // Inicjalizacja komunikacji szeregowej
  Serial.begin(115200);
  Serial.println("Rozpocznij wpisywanie sekwencji za pomocą przycisku BOOT:");

  // Proces ustawiania sekwencji
  while (userSequence.length() < sequenceLength) {
    // Odczytanie stanu przycisku
    buttonState = digitalRead(buttonPin) == LOW;  // LOW oznacza, że przycisk jest wciśnięty
    
    // Jeśli przycisk został naciśnięty, zarejestruj czas naciśnięcia
    if (buttonState && !lastButtonState) {
      buttonPressTime = millis();  // Rejestracja czasu wciśnięcia przycisku
    }
    
    // Jeśli przycisk został zwolniony, sprawdź długość naciśnięcia
    if (!buttonState && lastButtonState) {
      unsigned long pressDuration = millis() - buttonPressTime;  // Oblicz długość naciśnięcia
      if (pressDuration < 1000) {  // Krótkie naciśnięcie (mniej niż 1 sekunda)
        userSequence += "0";  // Dodaj "0" do sekwencji
        Serial.println("Dodano: 0");
      } else {  // Długie naciśnięcie (ponad 1 sekunda)
        userSequence += "1";  // Dodaj "1" do sekwencji
        Serial.println("Dodano: 1");
      }
      
      // Miganie diodą LED w celu sygnalizacji
      signalInput();
    }

    // Zapisanie ostatniego stanu przycisku
    lastButtonState = buttonState;
  }

  // Po zakończeniu wprowadzania sekwencji, wyświetlamy ją
  Serial.print("Wprowadzona sekwencja: ");
  Serial.println(userSequence);
  Serial.println("Sekwencja została ustawiona.");
}

void loop() {

Serial.print("Sekwencja: ");
Serial.println(userSequence);
delay(5000);
//logika programu

}

// Funkcja do migania diodą LED w celu sygnalizacji
void signalInput() {
  int ledPin = 2;  // Zwykle dioda LED jest podłączona do GPIO2
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);  // Włącz diodę LED
  delay(100);
  digitalWrite(ledPin, LOW);  // Wyłącz diodę LED
  delay(100);
}

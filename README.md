

## Przebieg gry

## Od strony klienta

1. klient A  wybieraja sekwencje orłów i reszek o długości n
2. klient B wybiera o tej samej długości
3. wygrywa ten kogo schemat wypadł pierwszy w wylosowanej sekwencji np **OOR** w OROR**OOR**RRO

## od strony serwera:

1. Rozpoczynanie gier jedna po drugiej (for i< n)

   1. Wiadomość o rozpoczęciu (header S)

   1. While(ktoś nie wygrał)
      1. Wypisanie informacji i sumowanie ich globalnie:
         1. the number of players
         2. the length of the patterns (n)
         3. how many times the game was played
         4. the probabilities of winning (estimate)
         5. the average number of tosses (estimate)
      2. wyślij wiadomość z nowym bitem 
      3. sprawdz czy ktoś nie wysłał informacji że wypadła jego sekwencja

   3. Zapisz wygrywającą sekwencje

3. Policz jakie prawdopodobieństwo zwycięstwa miała wygrywająca sekwencja(wystarczy wziąć  srednią z wielu gier) 

   



# TODO

- [ ] Serwer
  - [ ] VM na chmurze
    - [ ] firewall
    - [ ] instalacja systemu
  - [ ] Alogorytm
    - [ ] powtórzenie wiele razy(wyłonienie kto miał większą szanse na wygraną)
  - [ ] Transmisja
    - [ ] Protokół 
      - [ ] Headery, odpowiadanie ack na każdą wiadomość
        - [ ] start (S)
        - [ ] winner (W)  potem 0 - przegrałeś 1 - wygrałeś	
        - [ ] Register (R), id - rejestrowanie hosta z jego ip,portem i id
        - [ ] Game(G) - po nim kolejne bity oznaczjące wyrzuconą sekwencje
        - [ ] dodawanie ich do wychodzących połączeń
        - [ ] odejmownie od przychodzących i rodzielanie ich na tej podstawie
      - [ ] Kodowanie bitowe orła reszki na 0 i 1 
      - [ ] Uniwersalny dla klienta i serwera mechanizm czekania na potwierdzenie odebrania informacji ACK
    - [ ] zwracanie po skończonej grze zwycięscy oraz ilości rzutów potrzebnych do rozstrzygnięcia gry
- [ ] Klient
  - [ ] obsługa GPIO
    - [ ] Guziki do wklikiwania orzeł/reszka
    - [ ] świecenie diodą gdy się kliknie, szybko jak się wygra i wolno jak się przegra
  - [ ] Informowanie o starcie gry

## Sprawko

1. a specification of your ALP
▪ produce message formats and binary encodings for the protocol messages
• when specifying message formats, follow a good example, e.g., RTP or IP
▪ produce sequence diagrams (see the UML language to learn about sequence diagrams)
▪ describe how you deal with the fact that UDP is unreliable
2. a description of your server implementation
▪ components (overview of the server architecture)
▪ major data structures
3. a description of your node implementation
▪ include a description of your environment files
4. estimates obtained for some sets of patterns
1.probability of winning for each pattern
2.average number of tosses




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

- [x] Serwer
  - [x] VM na chmurze
    - [x] firewall
    - [x] instalacja systemu
  - [x] Alogorytm
    - [x] funkcja generująca sekwencje
    - [x] funkcja decydująca zwycięsce
    - [x] powtórzenie wiele razy(wyłonienie kto miał większą szanse na wygraną)
  - [x] Transmisja
    - [x] Protokół
      - [x] Headery, odpowiadanie ack na każdą wiadomość
        - [x] start (S)
        - [x] winner (W)  potem 0 - przegrałeś 1 - wygrałeś
        - [x] Register (R), id - rejestrowanie hosta z jego ip,portem i id
        - [x] Game(G) - po nim kolejne bity oznaczjące wyrzuconą sekwencje
        - [x] dodawanie ich do wychodzących połączeń
        - [x] odejmownie od przychodzących i rodzielanie ich na tej podstawie
      - [x] Kodowanie bitowe orła reszki na 0 i 1
      - [x] Uniwersalny dla klienta i serwera mechanizm czekania na potwierdzenie odebrania informacji ACK
    - [x] zwracanie po skończonej grze zwycięscy oraz ilości rzutów potrzebnych do rozstrzygnięcia gry
- [x] Klient
  - [x] obsługa GPIO
    - [x] Guziki do wklikiwania orzeł/reszka
    - [x] świecenie diodą gdy się kliknie, 3 razy wygra 
  - [x] Transmisja - obsługa wszyskich headerów wysyłanych przez serwer oraz:
    - [x] sEquence(E) wysyłanie sekwencji do serwera


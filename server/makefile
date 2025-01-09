# Kompilator
CC = gcc

# Flagi kompilatora
CFLAGS = -Wall -Wextra -std=c11

# Nazwa pliku wykonywalnego
TARGET = main

# Pliki źródłowe
SRC = game.c

# Domyślny cel - kompilacja programu
all: $(TARGET)

# Reguła kompilacji
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Cel do uruchomienia programu
run: $(TARGET)
	./$(TARGET)

# Cel do czyszczenia plików kompilacji
clean:
	rm -f $(TARGET)

# Cel phony, aby upewnić się, że nie koliduje z plikami o tych nazwach
.PHONY: all run clean

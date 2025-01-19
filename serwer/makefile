# Kompilator
CC = gcc

# Flagi kompilatora
CFLAGS = -Wall -Wextra -std=c11

# plik docelowy
TARGET = serwer

# nazwa komilowanego pliku
SRC = serwer_binary.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Cel do czyszczenia plików kompilacji
clean:
	rm -f $(TARGET)

# Cel phony, aby upewnić się, że nie koliduje z plikami o tych nazwach
.PHONY: all run clean

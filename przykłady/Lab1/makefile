# Define compiler and target files
CC = gcc
SERVER = server
CLIENT = client

# Define server ports and IDs
SERVER1_PORT = 1306
SERVER1_ID = 1000
SERVER2_PORT = 1307
SERVER2_ID = 2000

# Compile both server and client
all: $(SERVER) $(CLIENT)

$(SERVER): server.c
	$(CC) -o $(SERVER) server.c

$(CLIENT): client.c
	$(CC) -o $(CLIENT) client.c

# Run two servers and client in separate terminals
run: all
	gnome-terminal -- bash -c "./$(SERVER) $(SERVER1_PORT) $(SERVER1_ID); exec bash"
	gnome-terminal -- bash -c "./$(SERVER) $(SERVER2_PORT) $(SERVER2_ID); exec bash"
	sleep 1  # Wait for servers to start
	gnome-terminal -- bash -c "./$(CLIENT); exec bash"

# Clean up the compiled binaries
clean:
	rm -f $(SERVER) $(CLIENT)

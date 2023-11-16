#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 58920
#define WINDOW_WIDTH 1500
#define WINDOW_HEIGHT 900
#define MAX_CLIENTS 4

int main() {
    // Create a server socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Error creating server socket");
        exit(EXIT_FAILURE);
    }

    // Set up the server address struct
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    // Bind the server socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        perror("Error binding server socket");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(serverSocket, 1) == -1) {
        perror("Error listening for connections");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Accept a client connection
    int clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == -1) {
        perror("Error accepting client connection");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    int x = rand() % WINDOW_WIDTH;
    int y = rand() % WINDOW_HEIGHT;

    // Send coordinates to the client
    send(clientSocket, &x, sizeof(int), 0);
    send(clientSocket, &y, sizeof(int), 0);
    // Main loop

    while (1) {
        
    }

    // Clean up and close sockets
    close(clientSocket);
    close(serverSocket);

    return 0;
}
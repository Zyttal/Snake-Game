#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 58920
#define WINDOW_WIDTH 1500
#define WINDOW_HEIGHT 900
#define MAX_CLIENTS 4

typedef struct{
    int clientSocket;
    int playerID;
} ClientInfo;

void *clientHandler(void *arg) {
    int clientSocket = *((int *)arg);
    free(arg);

    int x = rand() % WINDOW_WIDTH;
    int y = rand() % WINDOW_HEIGHT;

    // Send starting coordinates to the client
    send(clientSocket, &x, sizeof(int), 0);
    send(clientSocket, &y, sizeof(int), 0);

    // Handle ongoing player movements from the client
    while (1) {
        int deltaX, deltaY;
        int bytesReceivedX = recv(clientSocket, &deltaX, sizeof(int), 0);
        int bytesReceivedY = recv(clientSocket, &deltaY, sizeof(int), 0);

        if (bytesReceivedX <= 0 || bytesReceivedY <= 0) {
            printf("Client disconnected\n");
            break;
        }

        // printf("X: %d Y: %d\n", deltaX, deltaY);
        // Handle player movements here and update player positions if needed
    }

    // Close client socket after handling movements or disconnection
    close(clientSocket);
    return NULL;
}

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

    // Array for player positions and movements
    int playerPositions[MAX_CLIENTS][2];
    int playerMovements[MAX_CLIENTS][2];


    // player ID Counter
    int playerID = 1;

    while (1) {
        if (playerID <= MAX_CLIENTS) {
            // Accept a client connection
            int *clientSocket = malloc(sizeof(int));
            *clientSocket = accept(serverSocket, NULL, NULL);
            if (*clientSocket == -1) {
                perror("Error accepting client connection");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }

            // Create client info to pass to the thread
            ClientInfo *clientInfo = malloc(sizeof(ClientInfo));
            clientInfo->clientSocket = *clientSocket;
            clientInfo->playerID = playerID;

            // Create a thread for the client connection
            pthread_t clientThread;
            if (pthread_create(&clientThread, NULL, clientHandler, (void *)clientInfo) != 0) {
                perror("Error creating client thread");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }

            pthread_detach(clientThread);

            // Display player connection
            printf("Player %d connected\n", playerID);
            playerID++;
        } else {
            // Accepts a socket but then it would be denied
            int *deniedSocket = malloc(sizeof(int));
            *deniedSocket = accept(serverSocket, NULL, NULL);
            if (*deniedSocket == -1) {
                perror("Error accepting client connection");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }
            close(*deniedSocket);
            printf("Connection Denied: Max number of players reached\n");
        }
    }

    // Clean up and close sockets
    close(serverSocket);

    return 0;
}
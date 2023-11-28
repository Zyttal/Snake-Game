// #include <SDL2/SDL.h> // To add - Server as a Spectator
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 58501
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 700
#define MAX_CLIENTS 5 // -1 to get the actual Maximum - (which is 4...)
#define MAX_SNAKE_LENGTH 100
#define SNAKE_SEGMENT_DIMENSION 15

#define MIN_X 0
#define MAX_X (WINDOW_WIDTH - SNAKE_SEGMENT_DIMENSION) // Adjusted for the snake's head size
#define MIN_Y 0
#define MAX_Y (WINDOW_HEIGHT - SNAKE_SEGMENT_DIMENSION) // Adjusted for the snake's head size

// Structs
typedef struct{
    int clientSocket;
    int playerID;
} PlayerInfo;

typedef struct {
    int x;
    int y;
} SnakeSegment;

typedef struct {
    SnakeSegment head;
    SnakeSegment body[MAX_SNAKE_LENGTH - 1]; // -1 for excluding head
    int body_length;
    int isAlive;
} Snake;

typedef struct{
    int deltaX, deltaY;
} Movement;

typedef struct {
    int clientSocket;
    int playerID;
    Snake playerSnake;
    Movement playerMovement;
    int active;
} PlayerData;

// Global Variables/Arrays
int serverSocket;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
PlayerData players[MAX_CLIENTS];
int startSignal = 0;

void startServer();
void initPlayer(PlayerInfo *playerInfo, Snake *playerSnake, Movement *startingMovement);
void *playerHandler(void *arg);
void *inputHandler(void *arg);
void broadcastSnakes(int senderID, Snake* playerSnake);

// Temporary Functions //
void printGameStatus();
char* checkStatus(PlayerData currentPlayer, int playersAlive);

int main() {
    startServer();
    
    // player ID Counter
    int playerID = 1;

    while (1) {
        if (playerID < MAX_CLIENTS) {
            // Accept a client connection
            int *clientSocket = malloc(sizeof(int));
            *clientSocket = accept(serverSocket, NULL, NULL);
            if (*clientSocket == -1) {
                perror("Error accepting client connection");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }

            // Create client info to pass to the thread
            PlayerInfo *playerInfo = malloc(sizeof(PlayerInfo));
            playerInfo->clientSocket = *clientSocket;
            playerInfo->playerID = playerID;

            // Create a thread for the client connection
            pthread_t clientThread;
            if (pthread_create(&clientThread, NULL, playerHandler, (void *)playerInfo) != 0) {
                perror("Error creating client thread");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }

            pthread_detach(clientThread);

            printGameStatus();
            playerID++;
        } else {
            // Accepts a socket then Denies Socket
            int *deniedSocket = malloc(sizeof(int));
            *deniedSocket = accept(serverSocket, NULL, NULL);
            if (*deniedSocket == -1) {
                perror("Error accepting client connection");
                close(serverSocket);
                exit(EXIT_FAILURE);
            }
            perror("Connection Denied: Max number of players reached\n");
            close(*deniedSocket);
        }
    }

    // Clean up and close sockets
    close(serverSocket);

    return 0;
}

void *playerHandler(void *arg) {
    PlayerInfo *playerInfo = (PlayerInfo *)arg;
    int clientSocket = playerInfo->clientSocket;
    int playerID = playerInfo->playerID;
    int deathFlag = 0;

    Snake playerSnake;
    Movement startingPosition;
    initPlayer(playerInfo, &playerSnake, &startingPosition);

    send(clientSocket, &playerID, sizeof(int), 0);
    send(clientSocket, &playerSnake, sizeof(Snake), 0);
    send(clientSocket, &startingPosition, sizeof(Movement), 0);

    pthread_mutex_lock(&mutex);
    players[playerID - 1].clientSocket = clientSocket;
    players[playerID - 1].playerID = playerID;
    players[playerID - 1].playerSnake = playerSnake;
    players[playerID - 1].playerMovement = startingPosition;
    players[playerID - 1].active = 1;
    pthread_mutex_unlock(&mutex);

    while (1) {
        // Receive updated snake position from the client
        int playerID;
        Snake receivedSnake;

        recv(clientSocket, &playerID, sizeof(int), 0);        
        int bytesReceived = recv(clientSocket, &receivedSnake, sizeof(Snake), 0);

        // Handle disconnection or error
        if (bytesReceived <= 0) {
            printf("Player %d disconnected.\n", playerID);
            pthread_mutex_lock(&mutex);
            players[playerID - 1].active = 0;
            pthread_mutex_unlock(&mutex);
            break; // Exit the loop on disconnection
        }

        if (!(receivedSnake.isAlive) && deathFlag == 0){
            printGameStatus();
            deathFlag = 1;
        }

        // Update the server's representation of the client's snake
        pthread_mutex_lock(&mutex);
        players[playerID - 1].playerSnake = receivedSnake;
        pthread_mutex_unlock(&mutex);

        // Broadcast updated snake positions to other players
        broadcastSnakes(playerID, &receivedSnake);
    }

    close(clientSocket);
    free(arg);
    return NULL;
}

void initPlayer(PlayerInfo *playerInfo, Snake *playerSnake, Movement *startingMovement){
    playerSnake->body_length = 50;
    playerSnake->isAlive = 1;
    switch (playerInfo->playerID) {
        case 1: // Top-left
            playerSnake->head.x = SNAKE_SEGMENT_DIMENSION;
            playerSnake->head.y = 0;
            startingMovement->deltaX = 0;
            startingMovement->deltaY = SNAKE_SEGMENT_DIMENSION;

            // Adjust the initial body positions relative to the head - from the left side
            for (int i = 0; i < playerSnake->body_length; ++i) {
                playerSnake->body[i].x = playerSnake->head.x - (i + 1) * SNAKE_SEGMENT_DIMENSION;
                playerSnake->body[i].y = playerSnake->head.y; // Same Y-coordinate as the head
            }
            break;
        case 2: // Top-right
            playerSnake->head.x  = WINDOW_WIDTH - SNAKE_SEGMENT_DIMENSION;
            playerSnake->head.y  = 0;
            startingMovement->deltaX = 0;
            startingMovement->deltaY = -SNAKE_SEGMENT_DIMENSION;

            // Adjust the initial body positions relative to the head - from the right side
            for (int i = 0; i < playerSnake->body_length; ++i) {
                playerSnake->body[i].x = playerSnake->head.x + (i + 1) * SNAKE_SEGMENT_DIMENSION;
                playerSnake->body[i].y = playerSnake->head.y; // Same Y-coordinate as the head
            }
            break;
        case 3: // Bottom-left
            playerSnake->head.x  = 0;
            playerSnake->head.y  = WINDOW_HEIGHT - SNAKE_SEGMENT_DIMENSION;
            startingMovement->deltaX = 0;
            startingMovement->deltaY = SNAKE_SEGMENT_DIMENSION;

            // Adjust the initial body positions relative to the head - from the left side
            for (int i = 0; i < playerSnake->body_length; ++i) {
                playerSnake->body[i].x = playerSnake->head.x - (i + 1) * SNAKE_SEGMENT_DIMENSION;
                playerSnake->body[i].y = playerSnake->head.y; // Same Y-coordinate as the head
            }
            break;
        case 4: // Bottom-right
            playerSnake->head.x = WINDOW_WIDTH - SNAKE_SEGMENT_DIMENSION;
            playerSnake->head.y = WINDOW_HEIGHT - SNAKE_SEGMENT_DIMENSION;
            startingMovement->deltaX = 0;
            startingMovement->deltaY = -SNAKE_SEGMENT_DIMENSION;

            // Adjust the initial body positions relative to the head - from the right side
            for (int i = 0; i < playerSnake->body_length; ++i) {
                playerSnake->body[i].x = playerSnake->head.x + (i + 1) * SNAKE_SEGMENT_DIMENSION;
                playerSnake->body[i].y = playerSnake->head.y; // Same Y-coordinate as the head
            }
            break;
    }
}

void *inputHandler(void *arg) {
    char input[10];
    // Temporary Dashboard
    printGameStatus();

    while (1) {
        fgets(input, sizeof(input), stdin);
        if (strcmp(input, "quit\n") == 0) {
            system("clear");
            printf("Server shutting down...\n");
            close(serverSocket);
            exit(EXIT_SUCCESS);
        }
        if (strcmp(input, "start\n") == 0) {
            startSignal = 1;
            system("clear");
            printf("Game has Started!\n");
        }
        
    }
    return NULL;
}

void startServer(){
    // Create a server socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
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

    // Initialize the client information array
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        players[i].clientSocket = -1;
        players[i].playerID = -1;
        players[i].active = 0;
    }

    pthread_t inputThread;
    if (pthread_create(&inputThread, NULL, inputHandler, NULL) != 0) {
        perror("Error creating input thread");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
}

void broadcastSnakes(int senderID, Snake* playerSnake) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (players[i].active && i != senderID - 1) {
            send(players[i].clientSocket, &startSignal, sizeof(int), 0);
            send(players[i].clientSocket, &senderID, sizeof(int), 0);
            send(players[i].clientSocket, playerSnake, sizeof(Snake), 0);
        }
    }
    pthread_mutex_unlock(&mutex);
}

// Temporary Functions //
void printGameStatus(){
    int playersAlive = 0;
    for(int i = 0; i < 3; i++){ if(players[i].playerSnake.isAlive) playersAlive++; }
    
    system("clear");
    printf("+---------------------------------+\n");
    printf("| S N A K E  G A M E  S E R V E R |\n");
    printf("+---------------------------------+\n");
    printf("| type start to Start the Game!   |\n");
    printf("| type quit to Quit the Server!   |\n");
    printf("+---------------------------------+\n");
    printf("| Player 1: %s         |\n", checkStatus(players[0], playersAlive));
    printf("| Player 2: %s         |\n", checkStatus(players[1], playersAlive));
    printf("| Player 3: %s         |\n", checkStatus(players[2], playersAlive));
    printf("| Player 4: %s         |\n", checkStatus(players[3], playersAlive));
    printf("+---------------------------------+\n");
}

char* checkStatus(PlayerData currentPlayer, int playersAlive){
    if(!currentPlayer.active){
        return "Connecting...";
    }else if(currentPlayer.playerSnake.isAlive){
        return "Alive        ";
    }else if(!currentPlayer.playerSnake.isAlive){
        return "Dead         ";
    }else if(currentPlayer.playerSnake.isAlive && playersAlive == 1){
        return "Winner       ";
    }
    return "Unknown      ";
}
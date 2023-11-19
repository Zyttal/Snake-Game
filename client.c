#include <SDL2/SDL.h> // Version 2.0.20 was used in the development
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 58920
#define WINDOW_WIDTH 1500
#define WINDOW_HEIGHT 900
#define MAX_CLIENTS 4

typedef struct {
    int x, y, width, height;
} Rectangle; // Set for current Client

typedef struct{
    int x, y, width, height;
} OtherRectangle; // Set for the positions of other players

typedef struct{
    int deltaX, deltaY;
} Movement;

OtherRectangle otherPlayers[MAX_CLIENTS]; // An array of Rectangles that represent the players
int playerID;

void moveRectangle(Rectangle* rect, int deltaX, int deltaY){
    if (rect->x + deltaY >= 0 && rect->x + deltaY + rect->width <= WINDOW_WIDTH) {
        rect->x += deltaY;
    }
    if (rect->y + deltaX >= 0 && rect->y + deltaX + rect->height <= WINDOW_HEIGHT) {
        rect->y += deltaX;
    }
}

void *receiveThread(void *arg) {
    int clientSocket = *((int *)arg);
    while (1) {
        int receivedPlayerID = 0, receivedX = 0 , receivedY = 0;
        recv(clientSocket, &receivedPlayerID, sizeof(int), 0);
        recv(clientSocket, &receivedX, sizeof(int), 0);
        recv(clientSocket, &receivedY, sizeof(int), 0);

        // Update other player's positions or handle the data received here...
        if (receivedPlayerID != playerID && 
        receivedX != -1 && 
        receivedY != -1 &&
        receivedX != 0 && 
        receivedY != 0) {
            // Update other player's positions
            otherPlayers[receivedPlayerID - 1].x = receivedX;
            otherPlayers[receivedPlayerID - 1].y = receivedY;
            otherPlayers[receivedPlayerID - 1].width = 20;
            otherPlayers[receivedPlayerID - 1].height = 20;
            printf("Player %d - X: %d Y: %d\n", receivedPlayerID, receivedX, receivedY);
        } else if (receivedX == -1 && receivedY == -1) {
            // Player disconnect handling
            otherPlayers[receivedPlayerID - 1].x = -1;
            otherPlayers[receivedPlayerID - 1].y = -1;
        }
    }
    return NULL;
}

int main() {
    // Create a client socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        perror("Error creating client socket");
        exit(EXIT_FAILURE);
    }

    // Set up the server address struct
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        perror("Error connecting to server");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    // Initialize SDL and then Creates SDL window and renderer
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    
    SDL_Window* window = SDL_CreateWindow("Snake Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(renderer == NULL){
        printf("Render did not load! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Create a thread for receiving data from the server
    pthread_t recvThread;
    if (pthread_create(&recvThread, NULL, receiveThread, &clientSocket) != 0) {
        perror("Error creating receive thread");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    int otherPlayerPositions[MAX_CLIENTS][2];

    // get PlayerID and initial position from server
    int startX, startY;
    recv(clientSocket, &playerID, sizeof(int), 0);
    recv(clientSocket, &startX, sizeof(int), 0);
    recv(clientSocket, &startY, sizeof(int), 0);

    // Initialize Current player's rectangle and set Directions
    Rectangle player = {startX, startY, 20,20};
    Movement playerDirection = {0, 20};

    SDL_Event event;
    int quit = 0;
    // Main Loop for SDL Events
    while (!quit) {
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                quit = 1;
            }else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP:
                        playerDirection.deltaX = -20;
                        playerDirection.deltaY = 0;
                        break;
                    case SDLK_DOWN:
                        playerDirection.deltaX = 20;
                        playerDirection.deltaY = 0;
                        break;
                    case SDLK_LEFT:
                        playerDirection.deltaX = 0;
                        playerDirection.deltaY = -20;
                        break;
                    case SDLK_RIGHT:
                        playerDirection.deltaX = 0;
                        playerDirection.deltaY = 20;
                        break;
                }
            }
        }

        moveRectangle(&player, playerDirection.deltaX, playerDirection.deltaY);

        // Clear the Screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        send(clientSocket, &player.x, sizeof(int), 0);
        send(clientSocket, &player.y, sizeof(int), 0);

        // Move and render the current Players Rectangle
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect rect = { player.x, player.y, player.width, player.height};
        SDL_RenderFillRect(renderer, &rect);

        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (otherPlayers[i].x != -1 && otherPlayers[i].y != -1) {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Set color for other players
                SDL_Rect otherRect = {
                    otherPlayers[i].x,
                    otherPlayers[i].y,
                    otherPlayers[i].width,
                    otherPlayers[i].height
                };
                SDL_RenderFillRect(renderer, &otherRect);
            }
        }
        
        // Updates the Screen
        SDL_RenderPresent(renderer);
        SDL_Delay(100);
    }

    // Destroys the Objects used in the program
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    // Clean up and close socket
    close(clientSocket);

    return 0;
}
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 58920
#define WINDOW_WIDTH 1500
#define WINDOW_HEIGHT 900

int main() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

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

    // Create SDL window and renderer
    SDL_Window* window = SDL_CreateWindow("Snake Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(renderer == NULL){
        printf("Render did not load! SDL_Error: %s\n", SDL_GetError());
    }
    SDL_SetRenderDrawColor(renderer, 255,255,255,255);


    SDL_Event e;
    int quit = 0;
    
    // Main loop
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }
        int x, y;

        // Receive coordinates from the server
        if (recv(clientSocket, &x, sizeof(int), 0) <= 0 ||
            recv(clientSocket, &y, sizeof(int), 0) <= 0) {
            perror("Error receiving coordinates from server");
            break;
        }

        printf("X Coordinate: %d\n", x);
        printf("Y Coordinate: %d\n", y);

        int squareSize = 20;
        SDL_Rect rect = { x - squareSize / 2, y - squareSize / 2, squareSize, squareSize };
        SDL_RenderFillRect(renderer, &rect);

        // Present the renderer
        SDL_RenderPresent(renderer);

        // Handle SDL events if needed...

        SDL_Delay(100);
    }

    // Destroys the Objects used in the program
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    // Quit SDL
    SDL_Quit();

    // Clean up and close socket
    close(clientSocket);

    return 0;
}

        // // Clear the screen
        // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        // SDL_RenderClear(renderer);

        // Draw a pixel at the received coordinates
        // SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        // SDL_RenderDrawPoint(renderer, x, y);
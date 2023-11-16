#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 58920
#define WINDOW_WIDTH 1500
#define WINDOW_HEIGHT 900

typedef struct {
    int x, y, width, height;
} Rectangle;

typedef struct{
    int x, y;
} StartingCoordinates;

void moveRectangle(Rectangle* rect, int deltaX, int deltaY){
    rect->x += deltaY;
    rect->y += deltaX;
}

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

    Rectangle player = {100, 100, 20,20};

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
                        moveRectangle(&player, -10, 0);
                        break;
                    case SDLK_DOWN:
                        moveRectangle(&player, 10, 0);
                        break;
                    case SDLK_LEFT:
                        moveRectangle(&player, 0, -10);
                        break;
                    case SDLK_RIGHT:
                        moveRectangle(&player, 0, 10);
                        break;
                }
            }
        }
        int x, y;

        int squareSize = 20;
        SDL_Rect rect = { player.x, player.y, player.width, player.height};


        SDL_RenderFillRect(renderer, &rect);
        SDL_RenderPresent(renderer);
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

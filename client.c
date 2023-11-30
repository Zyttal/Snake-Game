#include <SDL2/SDL.h> // Version 2.0.20 was used in the development
#include <SDL2/SDL_ttf.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/tcp.h>

#define PORT 58501
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 700
#define MAX_CLIENTS 5
#define MAX_SNAKE_LENGTH 100
#define SNAKE_SEGMENT_DIMENSION 15

#define MIN_X 0
#define MAX_X (WINDOW_WIDTH - SNAKE_SEGMENT_DIMENSION) // Adjusted for the snake's head size
#define MIN_Y 0
#define MAX_Y (WINDOW_HEIGHT - SNAKE_SEGMENT_DIMENSION) // Adjusted for the snake's head size

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

// Global Variables
Snake otherPlayers[MAX_CLIENTS];
int playerID;
int clientSocket;
int startSignal = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int win = 0;

// SDL Variables
SDL_Renderer* renderer;
SDL_Window* window;
TTF_Font* font;

SDL_Surface* deathTextSurface = NULL;
SDL_Texture* deathTextTexture = NULL;
SDL_Surface* waitingTextSurface = NULL;
SDL_Texture* waitingTextTexture = NULL;
SDL_Surface* winTextSurface = NULL;
SDL_Texture* winTextTexture = NULL;

// Function Prototypes
void *receiveThread(void *arg); // For receiving Broadcasted Snake Positions
void moveSnake(Snake *snake, Movement movement);
void handlePlayerInput(SDL_Event *event, Movement *playerDirection, int *quit, Movement *lastValidDirection, Snake *playerSnake);
void checkState(Snake* playerSnake, Snake* otherPlayers, int numOtherPlayers);
void initPlayerSnake(Snake *playerSnake, Movement *playerDirection);
void initConnection();

// SDL Function Prototypes
int initSDL();
void initSDL_ttf();
void renderAssets(SDL_Renderer* renderer, Snake* playerSnake, Snake* otherPlayers, int numOtherPlayers);
void *receiveThread(void *arg);
void showDeathMessage();
void showWaitingMessage();
void showWinMessage();

int main(){
    int numOtherPlayers = MAX_CLIENTS - 1;
    int quit = 0;
    Movement playerDirection;
    Snake playerSnake;
    SDL_Event event;

    initConnection();
    initSDL();
    initSDL_ttf();

    initPlayerSnake(&playerSnake, &playerDirection);

    // Create a thread for receiving data from the server
    pthread_t recvThread;
    if(pthread_create(&recvThread, NULL, receiveThread, &clientSocket) != 0) {
        perror("Error creating receive thread");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    // Waiting for Game Start
    while(1){
        if(startSignal == 1) {
            break;
        }
        
        renderAssets(renderer, &playerSnake, otherPlayers, numOtherPlayers);

        send(clientSocket, &playerID,sizeof(int), 0);
        send(clientSocket, &playerSnake, sizeof(Snake), 0);

        renderAssets(renderer, &playerSnake, otherPlayers, numOtherPlayers);
    }
    
    // Game Loop for SDL Events
    while(!quit){
        Movement lastValidDirection = playerDirection;
        handlePlayerInput(&event, &playerDirection, &quit, &lastValidDirection, &playerSnake);
        renderAssets(renderer, &playerSnake, otherPlayers, numOtherPlayers);

        if(playerSnake.isAlive){
            moveSnake(&playerSnake, playerDirection);
            
        }

        send(clientSocket, &playerID, sizeof(int), 0);
        send(clientSocket, &playerSnake, sizeof(Snake), 0);

        renderAssets(renderer, &playerSnake, otherPlayers, numOtherPlayers);
        
        SDL_RenderPresent(renderer);
        SDL_Delay(50);
        checkState(&playerSnake, otherPlayers, numOtherPlayers);
    }

    close(clientSocket);

    return 0;
}

int initSDL(){
    // Initialize SDL and then Creates SDL window and renderer
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    
    window = SDL_CreateWindow("Snake Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if(window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(renderer == NULL){
        printf("Render did not load! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
}

void initSDL_ttf(){
    if(TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init error: %s\n", TTF_GetError());
        return;
    }

    font = TTF_OpenFont("fonts/LiberationSans-Regular.ttf", 24); // Try "LiberationSans-Regular.ttf" if "DejaVuSans" is not available
    if(font == NULL) {
        fprintf(stderr, "Error loading font: %s\n", TTF_GetError());
        TTF_Quit();
        return;
    }

    SDL_Color deathTextColor = { 255, 0, 0 };
    deathTextSurface = TTF_RenderText_Solid(font, "You Died!", deathTextColor);
    if(deathTextSurface == NULL) {
        fprintf(stderr, "Error creating text surface: %s\n", TTF_GetError());
        TTF_CloseFont(font);
        TTF_Quit();
        return;
    }

    
    deathTextTexture = SDL_CreateTextureFromSurface(renderer, deathTextSurface);
    if(deathTextTexture == NULL) {
        fprintf(stderr, "Error creating text texture: %s\n", SDL_GetError());
        SDL_FreeSurface(deathTextSurface);
        TTF_CloseFont(font);
        TTF_Quit();
        return;
    }

    SDL_Color waitingTextColor = { 0, 0, 255 };
    waitingTextSurface = TTF_RenderText_Solid(font, "Waiting for Server...", waitingTextColor);
    if(waitingTextSurface == NULL) {
        fprintf(stderr, "Error creating text surface: %s\n", TTF_GetError());
        TTF_CloseFont(font);
        TTF_Quit();
        return;
    }

    waitingTextTexture = SDL_CreateTextureFromSurface(renderer, waitingTextSurface);
    if(waitingTextTexture == NULL) {
        fprintf(stderr, "Error creating text texture: %s\n", SDL_GetError());
        SDL_FreeSurface(waitingTextSurface);
        TTF_CloseFont(font);
        TTF_Quit();
        return;
    }

    SDL_Color winTextColor = { 0, 255, 0 };
    winTextSurface = TTF_RenderText_Solid(font, "You Win!", winTextColor);
    if(winTextSurface == NULL) {
        fprintf(stderr, "Error creating text surface: %s\n", TTF_GetError());
        TTF_CloseFont(font);
        TTF_Quit();
        return;
    }

    winTextTexture = SDL_CreateTextureFromSurface(renderer, winTextSurface);
    if(waitingTextTexture == NULL) {
        fprintf(stderr, "Error creating text texture: %s\n", SDL_GetError());
        SDL_FreeSurface(winTextSurface);
        TTF_CloseFont(font);
        TTF_Quit();
        return;
    }
}

void renderAssets(SDL_Renderer* renderer, Snake* playerSnake, Snake* otherPlayers, int numOtherPlayers) {
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Set background color
    SDL_RenderClear(renderer); // Clear the screen

    // Render the player's snake
    if(playerSnake->isAlive) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect headRect = { playerSnake->head.x, playerSnake->head.y, SNAKE_SEGMENT_DIMENSION, SNAKE_SEGMENT_DIMENSION };
        SDL_RenderFillRect(renderer, &headRect);
        for(int i = 0; i < playerSnake->body_length; ++i) {
            SDL_Rect bodyRect = { playerSnake->body[i].x, playerSnake->body[i].y, SNAKE_SEGMENT_DIMENSION, SNAKE_SEGMENT_DIMENSION };
            SDL_RenderFillRect(renderer, &bodyRect);
        }
    }
    // Render other players' snakes
    for(int i = 0; i < numOtherPlayers; ++i) {
        if(otherPlayers[i].isAlive){
                if(otherPlayers[i].head.x != -1 && otherPlayers[i].head.y != -1 && otherPlayers[i].body_length > 0) {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); 
                SDL_Rect otherHeadRect = { otherPlayers[i].head.x, otherPlayers[i].head.y, SNAKE_SEGMENT_DIMENSION, SNAKE_SEGMENT_DIMENSION };
                SDL_RenderFillRect(renderer, &otherHeadRect);
                for(int j = 0; j < otherPlayers[i].body_length; ++j) {
                    SDL_Rect otherBodyRect = { otherPlayers[i].body[j].x, otherPlayers[i].body[j].y, SNAKE_SEGMENT_DIMENSION, SNAKE_SEGMENT_DIMENSION };
                    SDL_RenderFillRect(renderer, &otherBodyRect);
                }
            }
        } 
    }

    // Render Messages
    if(!playerSnake->isAlive && !win){
        showDeathMessage();
    }
    if(!startSignal){
        showWaitingMessage();
    }
    if(win){
        showWinMessage();
    }

    // Update the window
    SDL_RenderPresent(renderer);
}

void *receiveThread(void *arg) {
    int clientSocket = *((int *) arg);
    while (1) {
        int receivedPlayerID;
        Snake receivedSnake;

        recv(clientSocket, &startSignal, sizeof(int), 0);
        recv(clientSocket, &receivedPlayerID, sizeof(int), 0);
        recv(clientSocket, &receivedSnake, sizeof(Snake), 0);

        if(receivedPlayerID != playerID){
            pthread_mutex_lock(&mutex);
            otherPlayers[receivedPlayerID - 1].head = receivedSnake.head;
            otherPlayers[receivedPlayerID - 1].body_length = receivedSnake.body_length;
            otherPlayers[receivedPlayerID - 1].isAlive = receivedSnake.isAlive;

            if(otherPlayers[receivedPlayerID - 1].body_length == receivedSnake.body_length) {
                memcpy(otherPlayers[receivedPlayerID - 1].body, receivedSnake.body, receivedSnake.body_length * sizeof(SnakeSegment));
            }else{
                for(int i = 0; i < otherPlayers[receivedPlayerID - 1].body_length; ++i){
                
                    otherPlayers[receivedPlayerID - 1].body[i].x = receivedSnake.body[i].x;
                    otherPlayers[receivedPlayerID - 1].body[i].y = receivedSnake.body[i].y;
                }
            }
            pthread_mutex_unlock(&mutex);
        } else if(receivedSnake.head.x == -1 && receivedSnake.head.y == -1) {
            // Player disconnect handling
            otherPlayers[receivedPlayerID - 1].head.x = -1;
            otherPlayers[receivedPlayerID - 1].head.y = -1;
            otherPlayers[receivedPlayerID - 1].body_length = 0;
        }
    }
    return NULL;
}

void moveSnake(Snake *snake, Movement movement) {
    // Move the body segments
    for(int i = snake->body_length - 1; i > 0; --i) {
        snake->body[i] = snake->body[i - 1]; // Move each body segment to the position of the segment before it
    }

    // Move the head
    SnakeSegment previousHead = snake->head; // Store the current head position
    snake->head.x += movement.deltaX;
    snake->head.y += movement.deltaY;

    if(snake->head.x < MIN_X || snake->head.x > MAX_X || snake->head.y < MIN_Y || snake->head.y > MAX_Y) {
        snake->isAlive = 0;
    }

    if(snake->isAlive) {
        for(int i = 0; i < MAX_CLIENTS; ++i) {
            // Check if Snake collides with itself
            for(int j = 0; j < snake->body_length; ++j){
                if(snake->head.x == snake->body[j].x &&
                snake->head.y == snake->body[j].y){
                    snake->isAlive = 0;
                    break;
                }
            }

            // Check collision with other snakes' heads
            if(otherPlayers[i].isAlive &&
                snake->head.x == otherPlayers[i].head.x &&
                snake->head.y == otherPlayers[i].head.y) {
                snake->isAlive = 0;
                break; // No need to check further
            }

            // Check collision with other snakes' bodies
            for(int j = 0; j < otherPlayers[i].body_length; ++j) {
                if(snake->head.x == otherPlayers[i].body[j].x &&
                    snake->head.y == otherPlayers[i].body[j].y &&
                    otherPlayers[i].isAlive) {
                    snake->isAlive = 0;
                    break; 
                }
            }

            if(!snake->isAlive) {
                break; // No need to check further if dead
            }
        }
    }

    // Move the first body segment to the previous head position
    snake->body[0] = previousHead;
}

void handlePlayerInput(SDL_Event *event, Movement *playerDirection, int *quit, Movement *lastValidDirection, Snake *playerSnake) {
    while (SDL_PollEvent(event) != 0) {
        if(event->type == SDL_QUIT) {
            playerSnake->isAlive = 0;
            *quit = 1;
        } else if(event->type == SDL_KEYDOWN) {
            Movement newDirection = *playerDirection;
            switch (event->key.keysym.sym) {
                case SDLK_UP:
                    newDirection.deltaX = 0;
                    newDirection.deltaY = -SNAKE_SEGMENT_DIMENSION;
                    break;
                case SDLK_DOWN:
                    newDirection.deltaX = 0;
                    newDirection.deltaY = SNAKE_SEGMENT_DIMENSION;
                    break;
                case SDLK_LEFT:
                    newDirection.deltaX = -SNAKE_SEGMENT_DIMENSION;
                    newDirection.deltaY = 0;
                    break;
                case SDLK_RIGHT:
                    newDirection.deltaX = SNAKE_SEGMENT_DIMENSION;
                    newDirection.deltaY = 0;
                    break;
            }
            
            // Check if the new direction is opposite to the last valid direction
            if(newDirection.deltaX != -lastValidDirection->deltaX || newDirection.deltaY != -lastValidDirection->deltaY) {
                // Update the player direction and last valid direction
                *playerDirection = newDirection;
                *lastValidDirection = *playerDirection;
            }
        }
    }
}

void initPlayerSnake(Snake *playerSnake, Movement *playerDirection){
    recv(clientSocket, &playerID, sizeof(int), 0);
    recv(clientSocket, playerSnake, sizeof(Snake), 0);
    recv(clientSocket, playerDirection, sizeof(Movement), 0);
}

void initConnection(){
    // Create a client socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSocket == -1) {
        perror("Error creating client socket");
        exit(EXIT_FAILURE);
    }

    // Set TCP_NODELAY option
    int flag = 1;
    int result = setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    if (result < 0) {
        perror("Couldn't setsockopt(TCP_NODELAY)");
        exit(EXIT_FAILURE);
    }

    // Set up the server address struct
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    inet_pton(AF_INET, "192.168.68.103", &serverAddress.sin_addr);

    // Connect to the server
    if(connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        perror("Error connecting to server");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }
}

void showDeathMessage() {
    if(deathTextTexture != NULL) {
        int textWidth = deathTextSurface->w;
        int textHeight = deathTextSurface->h;

        // Adjust coordinates to place the text at the bottom left corner
        SDL_Rect textRect = { 10, WINDOW_HEIGHT - textHeight - 10, textWidth, textHeight };

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Set background color

        SDL_RenderCopy(renderer, deathTextTexture, NULL, &textRect);
    }
}

void showWaitingMessage() {
    if(waitingTextTexture != NULL) {
        int textWidth = waitingTextSurface->w;
        int textHeight = waitingTextSurface->h;

        // Adjust coordinates to place the text at the bottom left corner
        SDL_Rect textRect = { 10, WINDOW_HEIGHT - textHeight - 10, textWidth, textHeight };

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Set background color

        SDL_RenderCopy(renderer, waitingTextTexture, NULL, &textRect);
    }
}

void showWinMessage() {
    if(waitingTextTexture != NULL) {
        int textWidth = winTextSurface->w;
        int textHeight = winTextSurface->h;

        // Adjust coordinates to place the text at the bottom left corner
        SDL_Rect textRect = { 10, WINDOW_HEIGHT - textHeight - 10, textWidth, textHeight };

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Set background color

        SDL_RenderCopy(renderer, winTextTexture, NULL, &textRect);
    }
}

void checkState(Snake* playerSnake, Snake* otherPlayers, int numOtherPlayers){
    if(startSignal == 0) return;
    if(win == 1) return;
    if(!playerSnake->isAlive) return;
    int aliveOtherPlayers = 0;

    // Check other players' alive status
    for(int i = 0; i < numOtherPlayers; ++i) {
        if(otherPlayers[i].isAlive) {
            aliveOtherPlayers++;
            // If at least one other player is alive, the game continues
            break;
        }
    }

    // If no other player is alive, declare the current player as the winner
    if(aliveOtherPlayers == 0) {
        win = 1;
    }
}
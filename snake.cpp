#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <string>

#include "neural_net.h"
#include "ai_vision.h"

using namespace std;

const int SCREEN_WIDTH = 600;
const int SCREEN_HEIGHT = 600;
const int BLOCK_SIZE = 20;

const int GRID_WIDTH = SCREEN_WIDTH / BLOCK_SIZE;
const int GRID_HEIGHT = SCREEN_HEIGHT / BLOCK_SIZE;

enum Direction { UP, DOWN, LEFT, RIGHT };

enum GameState { MENU, PLAYING, GAME_OVER, EXIT };

struct Segment {
    int x, y;
};

// ---------- AI ----------
// The trained network expects the same 11 -> 16 -> 3 shape used in train.cpp.
NeuralNet aiNet(NN_INPUT_SIZE, NN_HIDDEN_SIZE, NN_OUTPUT_SIZE);
bool aiWeightsLoaded = false;
bool aiMode = false;

snakecore::Direction toCoreDir(Direction d) {
    switch (d) {
        case UP: return snakecore::UP;
        case DOWN: return snakecore::DOWN;
        case LEFT: return snakecore::LEFT;
        default: return snakecore::RIGHT;
    }
}

Direction toLocalDir(snakecore::Direction d) {
    switch (d) {
        case snakecore::UP: return UP;
        case snakecore::DOWN: return DOWN;
        case snakecore::LEFT: return LEFT;
        default: return RIGHT;
    }
}

// Mirrors the live game into a snakecore::GameState so we can reuse the
// exact same input-encoding the network was trained on.
Direction aiChooseDirection(const vector<Segment>& snake, int foodX, int foodY, Direction dir) {
    snakecore::GameState gs;
    for (auto& s : snake) gs.snake.push_back({s.x, s.y});
    gs.foodX = foodX;
    gs.foodY = foodY;
    gs.dir = toCoreDir(dir);

    auto inputs = getInputs(gs);
    auto out = aiNet.forward(inputs);
    int action = NeuralNet::argmax(out);
    return toLocalDir(outputToDirection(action, gs.dir));
}

// ---------- TEXT ----------
void drawText(SDL_Renderer* renderer, TTF_Font* font,
              string text, int x, int y) {

    SDL_Color color = {255, 255, 255};

    SDL_Surface* surface =
        TTF_RenderText_Solid(font, text.c_str(), color);

    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect rect = {x, y, surface->w, surface->h};

    SDL_FreeSurface(surface);
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_DestroyTexture(texture);
}

// ---------- RESET GAME ----------
void resetGame(vector<Segment>& snake,
               int& foodX, int& foodY,
               int& score, int& speed,
               Direction& dir) {

    snake.clear();
    snake.push_back({10, 10});

    dir = RIGHT;

    foodX = rand() % GRID_WIDTH;
    foodY = rand() % GRID_HEIGHT;

    score = 0;
    speed = 120;
}

// ---------- MAIN ----------
int main(int argc, char* argv[]) {

    srand(time(0));

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow(
        "Snake Game",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    TTF_Font* font = TTF_OpenFont(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 28
    );

    vector<Segment> snake;
    int foodX, foodY;
    int score, speed;
    Direction dir;

    GameState state = MENU;
    bool running = true;

    SDL_Event event;

    resetGame(snake, foodX, foodY, score, speed, dir);

    // ---------- LOOP ----------
    while (running) {

        while (SDL_PollEvent(&event)) {

            if (event.type == SDL_QUIT)
                running = false;

            if (event.type == SDL_KEYDOWN) {

                // ===== MENU =====
                if (state == MENU) {

                    if (event.key.keysym.sym == SDLK_1) {
                        aiMode = false;
                        resetGame(snake, foodX, foodY, score, speed, dir);
                        state = PLAYING;
                    }

                    if (event.key.keysym.sym == SDLK_2)
                        state = EXIT;

                    if (event.key.keysym.sym == SDLK_3) {
                        std::cout << "[AI] Program compiled for "
                                  << NN_INPUT_SIZE << "-" << NN_HIDDEN_SIZE << "-" << NN_OUTPUT_SIZE
                                  << " network. Attempting to load best_weights.txt...\n";
                        aiWeightsLoaded = aiNet.loadFromFile("best_weights.txt");
                        if (aiWeightsLoaded) {
                            std::cout << "[AI] Weights loaded successfully.\n";
                            aiMode = true;
                            resetGame(snake, foodX, foodY, score, speed, dir);
                            speed = 80; // watchable pace regardless of trained difficulty ramp
                            state = PLAYING;
                        } else {
                            std::cout << "[AI] FAILED to load best_weights.txt "
                                      << "(missing file, wrong working directory, "
                                      << "or its header doesn't match "
                                      << NN_INPUT_SIZE << " " << NN_HIDDEN_SIZE << " " << NN_OUTPUT_SIZE << ").\n";
                        }
                    }
                }

                // ===== PLAYING =====
                else if (state == PLAYING) {

                    switch (event.key.keysym.sym) {
                        case SDLK_UP:
                            if (dir != DOWN) dir = UP;
                            break;
                        case SDLK_DOWN:
                            if (dir != UP) dir = DOWN;
                            break;
                        case SDLK_LEFT:
                            if (dir != RIGHT) dir = LEFT;
                            break;
                        case SDLK_RIGHT:
                            if (dir != LEFT) dir = RIGHT;
                            break;
                    }
                }

                // ===== GAME OVER =====
                else if (state == GAME_OVER) {

                    if (event.key.keysym.sym == SDLK_r) {
                        resetGame(snake, foodX, foodY,
                                  score, speed, dir);
                        state = PLAYING;
                    }

                    if (event.key.keysym.sym == SDLK_q)
                        state = EXIT;
                }
            }
        }

        // ---------- UPDATE ----------
        if (state == PLAYING) {

            if (aiMode)
                dir = aiChooseDirection(snake, foodX, foodY, dir);

            for (int i = snake.size() - 1; i > 0; i--)
                snake[i] = snake[i - 1];

            switch (dir) {
                case UP: snake[0].y--; break;
                case DOWN: snake[0].y++; break;
                case LEFT: snake[0].x--; break;
                case RIGHT: snake[0].x++; break;
            }

            // Wall collision
            if (snake[0].x < 0 || snake[0].x >= GRID_WIDTH ||
                snake[0].y < 0 || snake[0].y >= GRID_HEIGHT)
                state = GAME_OVER;

            // Self collision
            for (int i = 1; i < snake.size(); i++)
                if (snake[0].x == snake[i].x &&
                    snake[0].y == snake[i].y)
                    state = GAME_OVER;

            // Eat food
            if (snake[0].x == foodX &&
                snake[0].y == foodY) {

                snake.push_back({foodX, foodY});

                foodX = rand() % GRID_WIDTH;
                foodY = rand() % GRID_HEIGHT;

                score += 10;

                if (speed > 40)
                    speed -= 5;
            }
        }

        // ---------- RENDER ----------
        SDL_SetRenderDrawColor(renderer, 15, 15, 15, 255);
        SDL_RenderClear(renderer);

        // ===== MENU SCREEN =====
        if (state == MENU) {

            drawText(renderer, font,
                     "SNAKE GAME",
                     200, 150);

            drawText(renderer, font,
                     "1 - START",
                     220, 250);

            drawText(renderer, font,
                     "2 - QUIT",
                     220, 300);

            drawText(renderer, font,
                     "3 - AI PLAY",
                     220, 350);
        }

        // ===== PLAYING =====
        else if (state == PLAYING) {

            // Grid
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
            for (int i = 0; i < SCREEN_WIDTH; i += BLOCK_SIZE)
                SDL_RenderDrawLine(renderer, i, 0, i, SCREEN_HEIGHT);

            for (int i = 0; i < SCREEN_HEIGHT; i += BLOCK_SIZE)
                SDL_RenderDrawLine(renderer, 0, i, SCREEN_WIDTH, i);

            // Snake body
            SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
            for (int i = 1; i < snake.size(); i++) {
                SDL_Rect rect = {
                    snake[i].x * BLOCK_SIZE,
                    snake[i].y * BLOCK_SIZE,
                    BLOCK_SIZE,
                    BLOCK_SIZE
                };
                SDL_RenderFillRect(renderer, &rect);
            }

            // Head
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_Rect head = {
                snake[0].x * BLOCK_SIZE,
                snake[0].y * BLOCK_SIZE,
                BLOCK_SIZE,
                BLOCK_SIZE
            };
            SDL_RenderFillRect(renderer, &head);

            // Food
            SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
            SDL_Rect foodRect = {
                foodX * BLOCK_SIZE,
                foodY * BLOCK_SIZE,
                BLOCK_SIZE,
                BLOCK_SIZE
            };
            SDL_RenderFillRect(renderer, &foodRect);

            drawText(renderer, font,
                     (aiMode ? "AI - Score: " : "Score: ") + to_string(score),
                     10, 10);
        }

        // ===== GAME OVER =====
        else if (state == GAME_OVER) {

            drawText(renderer, font,
                     "GAME OVER",
                     200, 200);

            drawText(renderer, font,
                     "R - Restart",
                     200, 260);

            drawText(renderer, font,
                     "Q - Quit",
                     200, 310);
        }

        // ===== EXIT =====
        else if (state == EXIT) {
            running = false;
        }

        SDL_RenderPresent(renderer);

        SDL_Delay(speed);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}
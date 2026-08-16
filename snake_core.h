#pragma once
#include <vector>
#include <cstdlib>

// Pure game logic, no SDL. Used by both the headless GA trainer
// and (via the same rules) the real game, so the AI trains on
// exactly the rules it will later be judged on.
namespace snakecore {

const int GRID_WIDTH = 30;   
const int GRID_HEIGHT = 30;  

enum Direction { UP, DOWN, LEFT, RIGHT };

struct Segment { int x, y; };

struct GameState {
    std::vector<Segment> snake;
    int foodX = 0, foodY = 0;
    Direction dir = RIGHT;
    int score = 0;
    bool alive = true;
    int stepsSinceFood = 0;

    void spawnFood() {
        // simple retry-until-free spawn so food never lands on the snake
        int x, y;
        bool onSnake;
        do {
            x = rand() % GRID_WIDTH;
            y = rand() % GRID_HEIGHT;
            onSnake = false;
            for (auto& s : snake) if (s.x == x && s.y == y) { onSnake = true; break; }
        } while (onSnake);
        foodX = x; foodY = y;
    }

    void reset() {
        snake.clear();
        snake.push_back({GRID_WIDTH / 2, GRID_HEIGHT / 2});
        dir = RIGHT;
        score = 0;
        alive = true;
        stepsSinceFood = 0;
        spawnFood();
    }

    // Advances the game by one tick given the *intended* new direction.
    // Ignores a direct 180-degree reversal, same as the original game.
    void step(Direction newDir) {
        if (!((newDir == UP && dir == DOWN) || (newDir == DOWN && dir == UP) ||
              (newDir == LEFT && dir == RIGHT) || (newDir == RIGHT && dir == LEFT))) {
            dir = newDir;
        }

        for (int i = (int)snake.size() - 1; i > 0; i--)
            snake[i] = snake[i - 1];

        switch (dir) {
            case UP: snake[0].y--; break;
            case DOWN: snake[0].y++; break;
            case LEFT: snake[0].x--; break;
            case RIGHT: snake[0].x++; break;
        }

        stepsSinceFood++;

        // wall collision
        if (snake[0].x < 0 || snake[0].x >= GRID_WIDTH ||
            snake[0].y < 0 || snake[0].y >= GRID_HEIGHT) {
            alive = false;
            return;
        }

        // self collision
        for (size_t i = 1; i < snake.size(); i++) {
            if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
                alive = false;
                return;
            }
        }

        // food
        if (snake[0].x == foodX && snake[0].y == foodY) {
            snake.push_back({foodX, foodY});
            score++;
            stepsSinceFood = 0;
            spawnFood();
        }

        // starvation guard: without this, a network that learns to circle
        // forever without dying would "win" a fitness function based only
        // on survival time. This forces it to actually go get food.
        if (stepsSinceFood > 100 + (int)snake.size() * 20) {
            alive = false;
        }
    }
};
} 
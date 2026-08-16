#pragma once
#include <vector>
#include <algorithm>
#include "snake_core.h"

// Shared network shape -- both train.cpp and snake.cpp include this header,
// so they can never drift out of sync with each other. If you change these,
// you MUST retrain (old best_weights.txt will be rejected as incompatible).
const int NN_INPUT_SIZE = 24;
const int NN_HIDDEN_SIZE = 24;
const int NN_OUTPUT_SIZE = 3;

// 24 inputs:
//  [0..15]  8 compass directions (N, NE, E, SE, S, SW, W, NW), each giving
//           2 signals: how close the wall is, and how close its OWN BODY
//           is, looking straight along that ray (not just the adjacent
//           cell). This is what gives it real spatial awareness instead
//           of only reacting one step ahead.
//  [16..19] one-hot current heading (up, down, left, right)
//  [20..23] food is up / down / left / right relative to the head
//
// 3 outputs (relative to current heading):
//  0 = keep going straight, 1 = turn right, 2 = turn left
//
// NOTE: everything here is fully qualified with snakecore:: on purpose.
// Do NOT add "using namespace snakecore;" -- this header gets included
// into snake.cpp, which already has its own global Direction/Segment/
// GRID_WIDTH etc. from the original game, and pulling snakecore's names
// in unqualified makes the compiler unable to tell the two apart.

inline std::vector<double> getInputs(const snakecore::GameState& gs) {
    snakecore::Segment head = gs.snake[0];
    snakecore::Direction d = gs.dir;

    auto isBody = [&](int x, int y) {
        for (auto& s : gs.snake) if (s.x == x && s.y == y) return true;
        return false;
    };

    // 8 compass directions, clockwise from north
    static const int rayDirs[8][2] = {
        {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}
    };

    std::vector<double> in;
    in.reserve(NN_INPUT_SIZE);

    for (auto& rd : rayDirs) {
        int dx = rd[0], dy = rd[1];
        int x = head.x + dx, y = head.y + dy;
        int wallDist = 0;
        int bodyDist = -1; // -1 means "body not seen along this ray"

        while (x >= 0 && x < snakecore::GRID_WIDTH && y >= 0 && y < snakecore::GRID_HEIGHT) {
            wallDist++;
            if (bodyDist == -1 && isBody(x, y)) bodyDist = wallDist;
            x += dx; y += dy;
        }

        // Inverse distance: closer obstacle -> bigger number. This puts
        // the most decision-relevant signal (imminent danger) on a scale
        // the network can react to strongly, instead of a flat 0..1 ramp
        // where "20 cells away" and "25 cells away" look almost the same.
        double wallSignal = (wallDist > 0) ? 1.0 / wallDist : 1.0;
        double bodySignal = (bodyDist == -1) ? 0.0 : 1.0 / bodyDist;

        in.push_back(wallSignal);
        in.push_back(bodySignal);
    }

    in.push_back(d == snakecore::UP);
    in.push_back(d == snakecore::DOWN);
    in.push_back(d == snakecore::LEFT);
    in.push_back(d == snakecore::RIGHT);

    in.push_back(gs.foodY < head.y);
    in.push_back(gs.foodY > head.y);
    in.push_back(gs.foodX < head.x);
    in.push_back(gs.foodX > head.x);

    return in;
}

inline snakecore::Direction outputToDirection(int action, snakecore::Direction current) {
    static const snakecore::Direction clockwise[4] = {
        snakecore::UP, snakecore::RIGHT, snakecore::DOWN, snakecore::LEFT
    };
    int idx = 0;
    for (int i = 0; i < 4; i++) if (clockwise[i] == current) idx = i;
    if (action == 1) idx = (idx + 1) % 4;      // turn right
    else if (action == 2) idx = (idx + 3) % 4; // turn left
    return clockwise[idx];                      // action == 0: straight
}
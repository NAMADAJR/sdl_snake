#pragma once
#include <vector>
#include <algorithm>
#include <queue>
#include "snake_core.h"

// Shared network shape -- both train.cpp and snake.cpp include this header,
// so they can never drift out of sync with each other. If you change these,
// you MUST retrain (old best_weights.txt will be rejected as incompatible).
const int NN_INPUT_SIZE = 28;
const int NN_HIDDEN_SIZE = 28;
const int NN_OUTPUT_SIZE = 3;

// 28 inputs:
//  [0..15]  8 compass directions (N, NE, E, SE, S, SW, W, NW), each giving
//           2 signals: how close the wall is, and how close its OWN BODY
//           is, looking straight along that ray (not just the adjacent
//           cell). This is what gives it real spatial awareness instead
//           of only reacting one step ahead.
//  [16..19] one-hot current heading (up, down, left, right)
//  [20..23] food is up / down / left / right relative to the head
//  [24..25] head's ABSOLUTE position on the board (x, y), normalized 0..1.
//           The 8 rays above are all relative/local; this gives the
//           network a sense of "where am I on the map" -- e.g. near an
//           edge/corner vs. out in open space -- without needing to feed
//           in the entire 900-cell grid.
//  [26..27] food's ABSOLUTE position on the board (x, y), normalized 0..1.
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

    // absolute positions, normalized 0..1 -- gives global "where am I /
    // where's the food on the map" context the local rays can't provide
    in.push_back((double)head.x / (snakecore::GRID_WIDTH - 1));
    in.push_back((double)head.y / (snakecore::GRID_HEIGHT - 1));
    in.push_back((double)gs.foodX / (snakecore::GRID_WIDTH - 1));
    in.push_back((double)gs.foodY / (snakecore::GRID_HEIGHT - 1));

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

// ---------- Flood-fill safety layer ----------
// The network is purely reactive (see file header) -- it can pick a move
// that's safe THIS tick but seals off a pocket of the board it'll be
// trapped in a few ticks later. This layer catches that class of mistake
// without touching the network at all: after the network suggests a move,
// simulate it and flood-fill count how much open space remains reachable.
// If that space is smaller than the snake's own body, the move is very
// likely a self-trap, so fall back to whichever of the 3 possible moves
// leaves the most room -- but ONLY when the network's own pick looks
// risky, so this never overrides normal, healthy play.

inline int floodFillOpenSpace(const snakecore::GameState& gs) {
    const int W = snakecore::GRID_WIDTH, H = snakecore::GRID_HEIGHT;

    // Reused across calls -- avoids the heap-allocation cost of building a
    // fresh 2D grid on every single candidate-move check (this function
    // can run up to 3x per game tick, so allocation cost adds up fast).
    static thread_local std::vector<char> blocked;
    static thread_local std::vector<char> visited;
    static thread_local std::vector<int> queueBuf;
    if ((int)blocked.size() != W * H) {
        blocked.resize(W * H);
        visited.resize(W * H);
        queueBuf.resize(W * H);
    }
    std::fill(blocked.begin(), blocked.end(), 0);
    std::fill(visited.begin(), visited.end(), 0);

    if (gs.snake.empty()) return 0;
    int hx = gs.snake[0].x, hy = gs.snake[0].y;
    if (hx < 0 || hx >= W || hy < 0 || hy >= H) return 0;

    for (auto& s : gs.snake)
        if (s.x >= 0 && s.x < W && s.y >= 0 && s.y < H) blocked[s.y * W + s.x] = 1;

    int qh = 0, qt = 0;
    queueBuf[qt++] = hy * W + hx;
    visited[hy * W + hx] = 1;
    int count = 0;
    static const int dx[4] = {0, 0, 1, -1};
    static const int dy[4] = {1, -1, 0, 0};

    while (qh < qt) {
        int idx = queueBuf[qh++];
        int x = idx % W, y = idx / W;
        count++;
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i], ny = y + dy[i];
            if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
                int nidx = ny * W + nx;
                if (!visited[nidx] && !blocked[nidx]) {
                    visited[nidx] = 1;
                    queueBuf[qt++] = nidx;
                }
            }
        }
    }
    return count;
}

// Takes the network's raw output scores (before argmax) plus the current
// game state, and returns a move that's both a good idea (per the network)
// AND doesn't look like a self-trap (per the flood fill).
inline snakecore::Direction chooseSafeDirection(const snakecore::GameState& gs,
                                                 const std::vector<double>& netOutput) {
    // rank the 3 actions by the network's own preference, best first
    int order[3] = {0, 1, 2};
    std::sort(order, order + 3, [&](int a, int b) { return netOutput[a] > netOutput[b]; });

    snakecore::Direction fallback = outputToDirection(order[0], gs.dir);
    int bestSpace = -1;
    snakecore::Direction bestDir = fallback;

    for (int i = 0; i < 3; i++) {
        snakecore::Direction candidate = outputToDirection(order[i], gs.dir);
        snakecore::GameState sim = gs; // deep copy, real state untouched
        sim.step(candidate);
        if (!sim.alive) continue; // immediately fatal, never pick this

        int space = floodFillOpenSpace(sim);

        // Trust the network's top pick as long as it isn't clearly a trap.
        if (i == 0 && space >= (int)gs.snake.size()) return candidate;

        if (space > bestSpace) {
            bestSpace = space;
            bestDir = candidate;
        }
    }
    return bestDir; // best space found among the 3, or the network's pick if all looked equally risky
}
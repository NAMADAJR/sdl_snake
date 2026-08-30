#pragma once
#include <cmath>
#include "snake_core.h"
#include "neural_net.h"
#include "ai_vision.h"

// Result of playing one full game with a given network.
struct GameResult {
    int score;      // food eaten -- the metric a human actually cares about
    int steps;      // ticks survived
    double fitness; // shaped training signal (see simulateGame below)
};

// Plays exactly one game start-to-finish using the given network (with the
// flood-fill safety wrapper active, same as real runtime play), and reports
// both the raw outcome and a "fitness" score used to guide evolution.
//
// Why fitness != just score*1000:
//   - Pure score-based fitness only gives feedback at the moment of eating.
//     For a random early network, food is rare, so almost every game scores
//     0 and evolution has nothing to compare -- it's all noise.
//   - We add "potential-based shaping": a small reward each step for moving
//     closer to food (and a small penalty for moving away). This turns a
//     sparse signal (score) into a dense one (distance-to-food every tick),
//     which gives evolution something to climb well before a network is
//     good enough to reliably eat anything.
//   - Steps survived is kept as a tiny tiebreaker only (weight 0.1) --
//     large enough to prefer a longer-surviving snake between two equal
//     scores, but far too small for a network to "win" by just camping
//     safely without ever eating.
inline GameResult simulateGame(NeuralNet& net, int maxSteps = 2000) {
    snakecore::GameState gs;
    gs.reset();
    int steps = 0;
    double shapingBonus = 0.0;

    while (gs.alive && steps < maxSteps) {
        // Distance is measured against THIS food position, before the
        // move. If the move eats it, food respawns elsewhere -- comparing
        // against a moving target would corrupt the signal, so we freeze
        // the target for this one comparison.
        int targetX = gs.foodX, targetY = gs.foodY;
        int prevDist = std::abs(gs.snake[0].x - targetX) + std::abs(gs.snake[0].y - targetY);

        auto inputs = getInputs(gs);
        auto out = net.forward(inputs);
        snakecore::Direction newDir = chooseSafeDirection(gs, out);
        gs.step(newDir);
        steps++;

        if (gs.alive) {
            int newDist = std::abs(gs.snake[0].x - targetX) + std::abs(gs.snake[0].y - targetY);
            shapingBonus += (prevDist - newDist); // positive if it got closer, negative if further
        }
    }

    GameResult r;
    r.score = gs.score;
    r.steps = steps;
    r.fitness = gs.score * 1000.0 + shapingBonus + steps * 0.1;
    return r;
}
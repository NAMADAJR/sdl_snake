#include "snake_core.h"
#include "neural_net.h"
#include "ai_vision.h"
#include "simulate.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>

// Standalone benchmark: loads a trained network and plays it many times,
// reporting real statistics instead of a single anecdotal game or a single
// training-log number. This exists because judging changes (flood-fill,
// global position inputs, fitness shaping, etc.) by watching one game or
// reading one generation's "best fitness" line is unreliable -- both are
// subject to the same food-placement luck this whole file is built to
// average out. Run this the same way before and after any change to get
// an honest before/after comparison.
//
// Usage:
//   ./evaluate                      -> 50 games, best_weights.txt
//   ./evaluate 200                  -> 200 games, best_weights.txt
//   ./evaluate 200 old_weights.txt  -> 200 games, a specific weights file

int main(int argc, char* argv[]) {
    int numGames = 50;
    std::string weightsFile = "best_weights.txt";
    if (argc > 1) numGames = std::atoi(argv[1]);
    if (argc > 2) weightsFile = argv[2];

    NeuralNet net(NN_INPUT_SIZE, NN_HIDDEN_SIZE, NN_OUTPUT_SIZE);
    if (!net.loadFromFile(weightsFile)) {
        std::cerr << "Failed to load '" << weightsFile << "'. Either the file is "
                  << "missing, or its header doesn't match this program's expected "
                  << "shape (" << NN_INPUT_SIZE << " " << NN_HIDDEN_SIZE << " "
                  << NN_OUTPUT_SIZE << ").\n";
        return 1;
    }

    std::vector<int> scores;
    std::vector<int> stepsList;
    scores.reserve(numGames);
    stepsList.reserve(numGames);

    for (int i = 0; i < numGames; i++) {
        GameResult r = simulateGame(net);
        scores.push_back(r.score);
        stepsList.push_back(r.steps);
    }

    std::vector<int> sortedScores = scores;
    std::sort(sortedScores.begin(), sortedScores.end());

    double avgScore = 0;
    for (int s : scores) avgScore += s;
    avgScore /= scores.size();

    double avgSteps = 0;
    for (int s : stepsList) avgSteps += s;
    avgSteps /= stepsList.size();

    double variance = 0;
    for (int s : scores) variance += (s - avgScore) * (s - avgScore);
    variance /= scores.size();
    double stddev = std::sqrt(variance);

    int minScore = sortedScores.front();
    int maxScore = sortedScores.back();
    int medianScore = sortedScores[sortedScores.size() / 2];

    std::cout << "Evaluated '" << weightsFile << "' over " << numGames << " games:\n";
    std::cout << "  avg score:    " << avgScore << " food\n";
    std::cout << "  median score: " << medianScore << " food\n";
    std::cout << "  min score:    " << minScore << " food\n";
    std::cout << "  max score:    " << maxScore << " food\n";
    std::cout << "  std dev:      " << stddev << "  (lower = more consistent, less luck-dependent)\n";
    std::cout << "  avg steps:    " << avgSteps << " ticks survived\n";

    return 0;
}
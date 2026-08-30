#include "snake_core.h"
#include "neural_net.h"
#include "ai_vision.h"
#include "simulate.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <ctime>
#include <string>

using namespace snakecore;

const int INPUTS = NN_INPUT_SIZE, HIDDEN = NN_HIDDEN_SIZE, OUTPUTS = NN_OUTPUT_SIZE;

// --- Tune these if training feels too slow or plateaus too fast ---
int POP_SIZE = 150;
int GENERATIONS = 300;
const double MUTATION_RATE = 0.08;
const double MUTATION_STRENGTH = 0.3;
const int ELITE_COUNT = 8;
const int MAX_STEPS = 2000;

// How many games to average per network per generation. Food placement is
// random, so a single game's outcome is noisy -- a network can get lucky
// or unlucky spawns independent of how good it actually is. Averaging
// over several games smooths that out, so evolution is comparing "genuine
// skill" rather than "who happened to get an easy food layout this time."
// Cost scales linearly with this number, so it's a direct speed/accuracy
// tradeoff -- 3 is a reasonable default; drop to 1 to match old behavior.
int GAMES_PER_EVAL = 3;

struct EvalResult {
    double fitness;  // averaged shaped fitness, used for selection
    double avgScore; // averaged raw food-eaten count, just for display
};

EvalResult evaluateNetwork(NeuralNet& net) {
    double totalFitness = 0.0, totalScore = 0.0;
    for (int i = 0; i < GAMES_PER_EVAL; i++) {
        GameResult r = simulateGame(net, MAX_STEPS);
        totalFitness += r.fitness;
        totalScore += r.score;
    }
    EvalResult e;
    e.fitness = totalFitness / GAMES_PER_EVAL;
    e.avgScore = totalScore / GAMES_PER_EVAL;
    return e;
}

NeuralNet crossover(const NeuralNet& a, const NeuralNet& b, std::mt19937& rng) {
    NeuralNet child(INPUTS, HIDDEN, OUTPUTS);
    std::uniform_int_distribution<int> pointDist(0, (int)a.weights.size() - 1);
    int point = pointDist(rng);
    for (size_t i = 0; i < child.weights.size(); i++)
        child.weights[i] = (i < (size_t)point) ? a.weights[i] : b.weights[i];
    return child;
}

void mutate(NeuralNet& net, std::mt19937& rng) {
    std::uniform_real_distribution<double> chance(0.0, 1.0);
    std::normal_distribution<double> noise(0.0, MUTATION_STRENGTH);
    for (auto& w : net.weights)
        if (chance(rng) < MUTATION_RATE) w += noise(rng);
}

int main(int argc, char* argv[]) {
    // Accepts: any mix of two numbers (population, generations) and/or
    // --resume, in any order. e.g. all of these work:
    //   ./train
    //   ./train --resume
    //   ./train 200 400
    //   ./train --resume 200 400
    //   ./train 200 --resume 400
    bool resume = false;
    std::vector<int> nums;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--resume") {
            resume = true;
        } else {
            nums.push_back(std::atoi(argv[i]));
        }
    }
    if (nums.size() > 0) POP_SIZE = nums[0];
    if (nums.size() > 1) GENERATIONS = nums[1];

    srand((unsigned)time(0));
    std::mt19937 rng(std::random_device{}());

    std::vector<NeuralNet> population;

    NeuralNet seed(INPUTS, HIDDEN, OUTPUTS);
    bool haveSeed = resume && seed.loadFromFile("best_weights.txt");

    if (haveSeed) {
        std::cout << "Resuming from best_weights.txt\n";
        int seededCount = (int)(POP_SIZE * 0.7);
        for (int i = 0; i < seededCount; i++) {
            NeuralNet n = seed;
            mutate(n, rng);
            population.push_back(n);
        }
        population.push_back(seed);
        while ((int)population.size() < POP_SIZE) {
            NeuralNet n(INPUTS, HIDDEN, OUTPUTS);
            n.randomize(rng);
            population.push_back(n);
        }
    } else {
        if (resume) std::cout << "No best_weights.txt found, starting fresh.\n";
        for (int i = 0; i < POP_SIZE; i++) {
            NeuralNet n(INPUTS, HIDDEN, OUTPUTS);
            n.randomize(rng);
            population.push_back(n);
        }
    }

    for (int gen = 0; gen < GENERATIONS; gen++) {
        std::vector<double> fitness(POP_SIZE);
        std::vector<double> avgScore(POP_SIZE);
        for (int i = 0; i < POP_SIZE; i++) {
            EvalResult e = evaluateNetwork(population[i]);
            fitness[i] = e.fitness;
            avgScore[i] = e.avgScore;
        }

        std::vector<int> idx(POP_SIZE);
        for (int i = 0; i < POP_SIZE; i++) idx[i] = i;
        std::sort(idx.begin(), idx.end(), [&](int a, int b) { return fitness[a] > fitness[b]; });

        double best = fitness[idx[0]];
        double avg = 0;
        for (double f : fitness) avg += f;
        avg /= POP_SIZE;

        std::cout << "Gen " << gen
                  << "  best fitness=" << best
                  << " (avg score=" << avgScore[idx[0]] << " food over " << GAMES_PER_EVAL << " games)"
                  << "  pop avg fitness=" << avg << std::endl;

        population[idx[0]].saveToFile("best_weights.txt");

        std::vector<NeuralNet> next;
        for (int i = 0; i < ELITE_COUNT; i++) next.push_back(population[idx[i]]);

        std::uniform_int_distribution<int> pick(0, std::max(1, POP_SIZE / 3) - 1);
        while ((int)next.size() < POP_SIZE) {
            int a = idx[pick(rng)];
            int b = idx[pick(rng)];
            NeuralNet child = crossover(population[a], population[b], rng);
            mutate(child, rng);
            next.push_back(child);
        }
        population = next;
    }

    std::cout << "Training complete. Best weights saved to best_weights.txt\n";
    return 0;
}

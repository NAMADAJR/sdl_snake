#pragma once
#include <vector>
#include <cmath>
#include <random>
#include <fstream>

// A tiny feedforward net: inputSize -> hiddenSize (tanh) -> outputSize (linear).
// Trained by evolution (genetic algorithm), not backprop, so all we need
// is a forward pass plus a flat weight vector we can mutate/crossover.
class NeuralNet {
public:
    int inputSize, hiddenSize, outputSize;
    std::vector<double> weights; // flattened: [W1 | b1 | W2 | b2]

    NeuralNet(int in, int hidden, int out)
        : inputSize(in), hiddenSize(hidden), outputSize(out) {
        weights.resize(weightCount());
    }

    int weightCount() const {
        return inputSize * hiddenSize + hiddenSize + hiddenSize * outputSize + outputSize;
    }

    void randomize(std::mt19937& rng) {
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        for (auto& w : weights) w = dist(rng);
    }

    std::vector<double> forward(const std::vector<double>& input) const {
        int idx = 0;
        std::vector<double> hidden(hiddenSize, 0.0);
        for (int h = 0; h < hiddenSize; h++) {
            double sum = 0.0;
            for (int i = 0; i < inputSize; i++) sum += input[i] * weights[idx++];
            sum += weights[idx++]; // bias
            hidden[h] = std::tanh(sum);
        }

        std::vector<double> output(outputSize, 0.0);
        for (int o = 0; o < outputSize; o++) {
            double sum = 0.0;
            for (int h = 0; h < hiddenSize; h++) sum += hidden[h] * weights[idx++];
            sum += weights[idx++]; // bias
            output[o] = sum;
        }
        return output;
    }

    static int argmax(const std::vector<double>& v) {
        int best = 0;
        for (size_t i = 1; i < v.size(); i++) if (v[i] > v[best]) best = (int)i;
        return best;
    }

    bool saveToFile(const std::string& path) const {
        std::ofstream f(path);
        if (!f) return false;
        f << inputSize << " " << hiddenSize << " " << outputSize << "\n";
        for (double w : weights) f << w << " ";
        return true;
    }

    bool loadFromFile(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        int in, h, out;
        f >> in >> h >> out;
        if (in != inputSize || h != hiddenSize || out != outputSize) return false;
        for (auto& w : weights) f >> w;
        return true;
    }
};
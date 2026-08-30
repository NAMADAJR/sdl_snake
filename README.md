# SDL Snake Game 

A classic Snake game built in **C++** using **SDL2** and **SDL_ttf**, featuring a fully functional menu, score display, speed increase, and restart/quit options — plus a **self-learning AI** that evolves its own strategy through a genetic algorithm, avoids trapping itself using a flood-fill safety net, and can play the game live. This project demonstrates real-time input handling, graphics rendering, game loop management, and neuroevolution in C++.

---

## Features

- Arrow key movement
- Snake grows as it eats food
- Food spawns randomly on a grid — guaranteed never to spawn inside the snake's own body
- Score displayed on screen
- Snake speed increases as it eats
- Start menu with **Start**, **Quit**, and **Watch AI Play**
- **Restart** and **Quit** options after Game Over
- Grid-based background for better visuals
- Simple and clean SDL2 graphics
- **AI mode**: a neural network, trained entirely through evolution (no external ML libraries), that plays the game on its own and gets progressively better with more training
- **Flood-fill safety net**: catches moves that look safe one step ahead but would trap the snake in a dead end a few ticks later
- **Objective evaluation tool**: benchmark any trained network over many games and get real statistics (average, median, min, max, standard deviation) instead of judging by a single anecdotal run

---

## Installation
**Requirements:** `SDL2` and `SDL2_ttf` installed on your system. The AI trainer and evaluator need no extra dependencies — they're plain C++, no graphics required.

### Ubuntu / WSL:
```bash
sudo apt update
sudo apt install g++ libsdl2-dev libsdl2-ttf-dev
```

### Compile the game:
```bash
g++ -O2 -std=c++17 -o snake_game snake.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf)
```

### Compile the AI trainer (optional — a pre-trained `best_weights.txt` is included):
```bash
g++ -O2 -std=c++17 -o train train.cpp
```

### Compile the evaluation tool (optional, for benchmarking a trained network):
```bash
g++ -O2 -std=c++17 -o evaluate evaluate.cpp
```

> **Note:** `-std=c++17` is required for all three — the flood-fill safety code uses a C++17 feature.

### Run:
```bash
./snake_game
```

---

##  Controls

### Menu
- `1` → Start (play manually)
- `2` → Quit
- `3` → Watch AI Play (loads `best_weights.txt`)

### Playing
- Arrow keys → Move snake

### Game Over
- `R` → Restart
- `Q` → Quit

---

## Training the AI

```bash
./train                    # fresh training run, defaults: 150 population, 300 generations
./train 200 500             # custom population size and generation count
./train --resume            # continue evolving from the current best_weights.txt instead of starting over
```

Training is headless (no window, no rendering delay), so it's lightweight — tens of thousands of simulated games typically finish in well under a minute on a modern laptop, though the flood-fill safety check and multi-game fitness averaging (see below) make each generation more expensive than a bare-bones version would be. Progress prints to the terminal each generation, e.g.:

```
Gen 7  best fitness=41358 (avg score=40.3333 food over 3 games)  pop avg fitness=22845
```

The best network found so far is saved to `best_weights.txt` automatically after every generation, so you can stop training at any time (even mid-run with Ctrl+C) without losing progress.

### Objectively evaluating a trained network

```bash
./evaluate                          # 50 games against best_weights.txt
./evaluate 200                      # 200 games against best_weights.txt
./evaluate 200 old_weights.txt       # 200 games against a specific weights file
```

This exists because judging an AI change by watching one game, or reading one generation's "best fitness" line, is unreliable — food placement is random, so a single result can be a lucky or unlucky outlier. `evaluate` plays a large number of fresh games and reports real statistics:

```
Evaluated 'best_weights.txt' over 100 games:
  avg score:    35.08 food
  median score: 38 food
  min score:    0 food
  max score:    43 food
  std dev:      10.5278  (lower = more consistent, less luck-dependent)
  avg steps:    1854.38 ticks survived
```

Use this before and after any change (a new fitness function, a vision upgrade, more training) to get an honest before/after comparison instead of an anecdote.

---

## How It Works

### The base game
- **Game Loop:** Continuously updates the game state, handles input, and renders graphics.
- **Snake Movement:** Stored as a vector of segments, with the head moving in the current direction and the body following.
- **Collision Detection:** Checks for wall collisions and self-collision to end the game.
- **Food System:** Spawns food on a random free cell, retrying until it finds one not occupied by the snake's body — guarantees food never spawns inside the snake.
- **Speed Scaling:** Game speed increases as the snake eats more food.
- **Menu System:** Implements a start menu, game over screen, and restart/quit functionality.

### The AI

**Perception (28 inputs, `ai_vision.h`):**
- **8-directional distance sensing (16 inputs):** the snake casts sensing rays in all 8 compass directions (including diagonals), each reporting two signals — how close the nearest wall is, and how close its own body is — along that ray. This gives it real spatial awareness of the area immediately around it, not just the single adjacent cell.
- **Current heading (4 inputs):** one-hot encoded up/down/left/right.
- **Relative food direction (4 inputs):** whether food is above, below, left, or right of the head.
- **Absolute position (4 inputs):** the head's own (x, y) on the board and the food's (x, y), both normalized 0–1. This gives the network global context — "where am I on the map" — that the purely local rays above can't provide, without needing to feed in the entire 900-cell grid (which would balloon the network to tens of thousands of weights and make evolution far slower to converge).

**Decision (28 → 28 → 3 network, `neural_net.h`):** a single hidden layer (tanh activation) processes the 28 inputs; the 3 outputs map to *turn left / go straight / turn right* relative to its current heading, not absolute directions — this means it never has to "learn" not to reverse into itself, since that move isn't representable.

**Flood-fill safety layer (`ai_vision.h`, `chooseSafeDirection`):** the network above is purely reactive — it evaluates the board fresh every tick with no memory of past states and no lookahead. That means it can pick a move that's safe *this* tick but seals off a pocket of the board it gets trapped in a few ticks later. To catch this class of mistake without touching the network itself: after the network suggests a move, the game simulates it and flood-fill counts how much open space remains reachable from the resulting position. If that space is smaller than the snake's own body length, the move is very likely a self-trap, so the game falls back to whichever of the 3 possible moves leaves the most room — but *only* when the network's own top pick looks risky, so healthy, normal play is never overridden.

**Learning (genetic algorithm, `train.cpp`):** no backpropagation, no labeled data — the network is bred, not trained in the traditional ML sense.
1. Start with a population of networks (150 by default), each with random weights (or seeded from `best_weights.txt` plus mutated variants, when using `--resume`).
2. Each network plays several full games (`GAMES_PER_EVAL = 3` by default, see "Reducing fitness noise" below) and is scored by a fitness function (see below).
3. The top performers (`ELITE_COUNT`) are copied unchanged into the next generation as insurance against losing progress to bad luck.
4. The rest of the new generation is filled by picking two parents (biased toward the top third of the ranking), splicing their weights together at a random point (crossover), then randomly nudging a fraction of the resulting weights (mutation).
5. Repeat for as many generations as you like — networks with better weight-patterns leave more descendants, generation after generation, until effective play strategies are baked in purely through differential survival.

**The fitness function (`simulate.h`):**
```
fitness = score × 1000 + shapingBonus + steps × 0.1
```
- `score × 1000` is the primary objective — food eaten dominates everything else.
- `shapingBonus` is **potential-based reward shaping**: every tick, the network gets a small reward if the snake's head moved closer to the food (by Manhattan distance) and a small penalty if it moved further away. Without this, a mostly-random early network almost never eats, so every game scores near-zero and evolution has no gradient to climb — shaping turns a sparse signal (score, which only changes at the rare moment of eating) into a dense one (distance-to-food, which changes every single tick), which lets evolution start making real progress far sooner.
- `steps × 0.1` is a small tiebreaker only — large enough to prefer a longer-surviving snake between two otherwise-equal scores, but far too small for a network to "win" by simply stalling/camping without ever eating.

**Reducing fitness noise (`train.cpp`, `GAMES_PER_EVAL`):** food placement is random every game, so a single game's fitness is noisy — a network can get a lucky or unlucky spawn sequence independent of how good its actual strategy is. Each network now plays `GAMES_PER_EVAL = 3` separate games per generation, and its selection fitness is the average across them. This smooths out luck considerably, so evolution ends up comparing genuine skill differences far more than who happened to get an easy food layout that round.

**Runtime (`snake.cpp`):** when you press `3`, the trained weights are loaded into an identically-shaped network, and each game tick it senses the board, runs a forward pass, passes the result through the same flood-fill safety check used during training, and applies the resulting move — the same logic used during training, just running once per frame instead of thousands of times per second.

**Known limitation:** even with 8-directional sensing, global position, and the flood-fill safety net, this is still fundamentally a reactive system with one-step-ahead lookahead (via the flood fill), not a true planner. It has no memory of past states and doesn't simulate multiple moves into the future, so it can still occasionally box itself into a bad position its immediate sensors and flood-fill check can't foresee — a deliberate simplicity/performance tradeoff rather than a bug.

---

## Skills Demonstrated

- **C++ Programming:** Loops, enums, vectors, structures, headers/shared code across multiple binaries
- **Game Development:** Game loop, collision detection, input handling
- **SDL2 Graphics:** Rendering rectangles, background grid, colors
- **SDL_ttf Text Rendering:** Score display and menu/game over text
- **Machine Learning / Neuroevolution:** Feedforward neural networks from scratch, genetic algorithms (selection, crossover, mutation, elitism), reward shaping, fitness noise reduction, headless simulation for fast training
- **Algorithms:** Flood-fill (BFS) for reachability/safety analysis, ray casting for spatial sensing
- **Empirical evaluation:** Building tooling (the `evaluate` binary) to objectively measure and compare model performance rather than relying on anecdotal observation
- **Project Organization:** Separate functions and headers for readability and maintainability, shared logic (`simulate.h`) reused identically across the trainer and the evaluator to guarantee consistent measurement

---

## Project Structure
```bash
sdl_snake_game/
├─ snake.cpp          # Main game code (manual play + AI-watch mode)
├─ snake_core.h        # Pure game rules shared by the trainer, evaluator, and the game
├─ neural_net.h         # Feedforward neural network (forward pass, save/load)
├─ ai_vision.h           # Board-state -> network-input encoding, output -> move decoding,
│                        # and the flood-fill safety layer
├─ simulate.h            # Shared "play one full game" logic + fitness function,
│                        # used identically by train.cpp and evaluate.cpp
├─ train.cpp             # Headless genetic algorithm trainer
├─ evaluate.cpp          # Objective benchmarking tool (avg/median/min/max/std dev over N games)
├─ best_weights.txt       # Trained network weights (auto-generated by train.cpp)
├─ README.md              # This file
```

---

## Future Upgrades

- Add sound effects `(SDL_mixer)`
- High score saving
- Multiple levels with obstacles
- Animated snake sprites
- Pause system
- Mobile or web port
- Live training visualization (watch fitness improve in real time inside the game window)
- Genuine multi-step lookahead/planning for the AI, beyond the current one-step flood-fill safety check
- Diversity injection during training (periodically introducing fresh random individuals) to help escape fitness plateaus
- Larger network capacity (more hidden neurons) to raise the ceiling on representable strategies

---

## Author

Project by **Namada Simoni**

GitHub: [Namada Jr](https://github.com/NAMADAJR)
# SDL Snake Game 🐍

A classic Snake game built in **C++** using **SDL2** and **SDL_ttf**, featuring a fully functional menu, score display, speed increase, and restart/quit options — plus a **self-learning AI** that evolves its own strategy through a genetic algorithm and can play the game live. This project demonstrates real-time input handling, graphics rendering, game loop management, and neuroevolution in C++.

---

##  Features

- Arrow key movement
- Snake grows as it eats food
- Food spawns randomly on a grid
- Score displayed on screen
- Snake speed increases as it eats
- Start menu with **Start**, **Quit**, and **Watch AI Play**
- **Restart** and **Quit** options after Game Over
- Grid-based background for better visuals
- Simple and clean SDL2 graphics
- **AI mode**: a neural network, trained entirely through evolution (no external ML libraries), that plays the game on its own and gets progressively better with more training

---

##  Installation
**Requirements:** `SDL2` and `SDL2_ttf` installed on your system. The AI trainer needs no extra dependencies — it's plain C++.

### Ubuntu / WSL:
```bash
sudo apt update
sudo apt install g++ libsdl2-dev libsdl2-ttf-dev
```

### Compile the game:
```bash
g++ -O2 -o snake_game snake.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf)
```

### Compile the AI trainer (optional — a pre-trained `best_weights.txt` is included):
```bash
g++ -O2 -o train train.cpp
```

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

##  Training the AI

```bash
./train                    # fresh training run, defaults: 150 population, 300 generations
./train 200 500             # custom population size and generation count
./train --resume            # continue evolving from the current best_weights.txt instead of starting over
```

Training is headless (no window, no rendering delay), so it's extremely lightweight — tens of thousands of simulated games typically finish in well under a minute on a modern laptop. Progress prints to the terminal each generation (`best fitness`, `avg fitness`), and the best network found so far is saved to `best_weights.txt` automatically after every generation, so you can stop training at any time without losing progress.

---

##  How It Works

### The base game
- **Game Loop:** Continuously updates the game state, handles input, and renders graphics.
- **Snake Movement:** Stored as a vector of segments, with the head moving in the current direction and the body following.
- **Collision Detection:** Checks for wall collisions and self-collision to end the game.
- **Food System:** Randomly spawns food, increases snake size and score when eaten.
- **Speed Scaling:** Game speed increases as the snake eats more food.
- **Menu System:** Implements a start menu, game over screen, and restart/quit functionality.

### The AI
The AI is a small feedforward neural network trained through **neuroevolution** (a genetic algorithm), not backpropagation — no labeled data, no gradient descent, just breeding better weight patterns over generations.

- **Perception (24 inputs):** the snake casts sensing rays in all 8 compass directions (including diagonals), measuring distance to the nearest wall and to its own body along each ray. It also knows its current heading and which general direction the food is in.
- **Decision (24 → 24 → 3 network):** a single hidden layer processes those 24 inputs; the 3 outputs map to *turn left / go straight / turn right* relative to its current heading, not absolute directions — this means it never has to "learn" not to reverse into itself, since that move isn't representable.
- **Learning (genetic algorithm):** each generation, every network in the population plays a full game, is scored mainly by food eaten (survival time as a tiebreaker), and the best performers are bred — their weights spliced together and randomly mutated — to form the next generation. Over hundreds of generations, this reliably evolves a network that eats dozens of food items per game from a starting point of pure random noise.
- **Runtime:** when you press `3`, the trained weights are loaded into an identically-shaped network, and each game tick it senses the board, runs a forward pass, and picks a move — the exact same logic used during training, just running once per frame instead of thousands of times per second.

This design is reactive rather than predictive: it evaluates the board fresh every tick with no memory of past states and no lookahead simulation, so it can still occasionally trap itself several moves ahead of what its sensors can detect — a deliberate simplicity/performance tradeoff, not a bug.

---

##  Skills Demonstrated

- **C++ Programming:** Loops, enums, vectors, structures, headers/shared code across multiple binaries
- **Game Development:** Game loop, collision detection, input handling
- **SDL2 Graphics:** Rendering rectangles, background grid, colors
- **SDL_ttf Text Rendering:** Score display and menu/game over text
- **Machine Learning / Neuroevolution:** Feedforward neural networks from scratch, genetic algorithms (selection, crossover, mutation, elitism), fitness design, headless simulation for fast training
- **Project Organization:** Separate functions and headers for readability and maintainability, shared logic reused across the trainer and the game

---

##  Project Structure
```bash
sdl_snake_game/
├─ snake.cpp          # Main game code (manual play + AI-watch mode)
├─ snake_core.h        # Pure game rules shared by the trainer and the game
├─ neural_net.h         # Feedforward neural network (forward pass, save/load)
├─ ai_vision.h           # Board-state -> network-input encoding, and output -> move decoding
├─ train.cpp            # Headless genetic algorithm trainer
├─ best_weights.txt       # Trained network weights (auto-generated by train.cpp)
├─ README.md            # This file
```

---

##  Future Upgrades

- Add sound effects `(SDL_mixer)`
- High score saving
- Multiple levels with obstacles
- Animated snake sprites
- Pause system
- Mobile or web port
- Live training visualization (watch fitness improve in real time inside the game window)
- Lookahead/planning for the AI instead of purely reactive sensing

---

##  Author

Project by **Namada Simoni**

GitHub: [Namada Jr](https://github.com/NAMADAJR)
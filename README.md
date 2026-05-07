# toaster_engine ♞

A custom C++ chess engine currently targeting strength beyond my own (~1500 rapid).

> Focused on search, pruning, and engine optimization techniques.

`Status:` Active Development

---

## Features

Current search and evaluation techniques implemented:

- Negamax Search
- Alpha-Beta Pruning
- Null Move Pruning
- Quiescence Search
- Iterative Deepening
- Aspiration Windows
- Transposition Tables
- Piece-Square Tables

### Move Ordering

- MVV-LVA
- Killer Moves
- History Heuristics
- Principal Variation Search

### Search Improvements

- Late Move Reductions (LMR)
- Check Extensions

---

## Build

### Using Makefile (recommended)

Build and run with compiler optimizations enabled:

- `-O3`
- `-march=native`
- `-flto`

```bash
make run
```

### Manual Compilation

```bash
g++ -std=c++20 -O3 -march=native -flto \
magic.cpp pst.cpp uci.cpp state_2.cpp \
-o engine
```

### Run

```bash
./engine
```

### Higher Priority Execution

| Platform | Command |
|---|---|
| Linux | `sudo nice -n -20 ./engine` |
| Windows | `start /high engine.exe` |

*Windows build/runtime not yet tested.*

---

## Requirements

- GCC with C++20 support
- Clang should work as well
- `make`
- Linux / Ubuntu environment recommended

---

## Protocol

The engine communicates using the UCI (Universal Chess Interface) protocol and can be connected to chess GUIs such as:

- Cute Chess *(preferred)*
- Arena
- Banksia GUI

### Supported UCI Commands

- `uci`
- `isready`

- `position`
  - `position startpos`
  - `position fen <FEN>`

- `go depth <DEPTH>`
- `go movetime <TIME(ms)>`

This allows the engine to interface with compatible chess GUIs for gameplay, testing, and search analysis.

---

## Roadmap

### Version 1 — Core Engine

Focus on implementing core search and pruning techniques to create a stable and functional engine.

### Version 2 — Optimization

Focus on:

- Better resource utilization
- Lower-level performance improvements
- Faster move generation and search efficiency
- Evaluation tuning

### Version 3 — TBD

To be decided.
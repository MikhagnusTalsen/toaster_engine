# toaster_engine ♞

A custom C++ chess engine currently targeting strength beyond my own (~1600 rapid).

Focused on search, pruning, and engine optimization techniques.

`Reports:` [Performance Report](PERFORMANCE.md)

## Contents
- [Feature](#features)
- [Build](#build)
- [Requirements](#requirements)
- [Protocol](#protocol)
- [Roadmap](#roadmap)

## Features

### Search

- Negamax Search
- Alpha-Beta Pruning
- Null Move Pruning (NMP)
- Quiescence Search 
- Iterative Deepening
- Aspiration Windows
- Transposition Tables
- Late Move Reductions (LMR)
- Check Extensions

### Move Ordering

- Most Valuable Victim - Least Valuable Attacker (MVV-LVA) 
- Killer Moves
- History Heuristics
- Principal Variation Search (PVS)

### Evaluation 

- Piece-Square Tables (PeSTO)
- Tapered Evaluation  

## Build

### Using Makefile (recommended)

Build and run with compiler optimizations enabled:

- `-O3`
- `-march=native`
- `-flto=auto`

```bash
make run
```

### Manual Compilation

```bash
g++ -std=c++20 -O3 -march=native -flto=auto \
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

- GCC with C++14 support (Clang should work as well)
- `make`
- Linux / Ubuntu environment recommended

---

## Protocol

The engine communicates using the UCI (Universal Chess Interface) protocol and can be connected to chess GUIs such as:

- Cute Chess *(preferred for lightweight)*
- Arena
- Banksia GUI

### Supported UCI Commands

This allows the engine to interface with compatible chess GUIs for gameplay, testing, and search analysis.

- `uci`
- `isready`

- `position`
  - `position startpos`
  - `position fen <FEN>`

- `go depth <DEPTH>`
- `go movetime <TIME(ms)>`
- `go wtime <TIME(ms)> btime <TIME(ms)> winc <TIME(ms)> binc <TIME(ms)>`

`depth` can be used together with  `movetime`, `wtime btime winc binc`.

---

## Roadmap

### Version 1 — Core Engine

Focus on implementing core search and pruning techniques to create a stable and functional engine.

### Version 2 — New Features and Optimization

Focus on:

- Better resource utilization
- Lower-level performance improvements
- Faster move generation and search efficiency
- NNUE Integration

### Version 3 — TBD

- Multithreading
- To be decided.

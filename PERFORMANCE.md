# PERFORMANCE

## Summary

* **Engine:** toaster
* **Version:** v1 
* **Estimated strength:** **2150 ± 100 Elo**
* **Benchmark platform:** AMD Ryzen 5 5625U (12 threads) @ 4.390 GHz, 16 GB DDR4 2400 MHz
* **Build flags:** `-O3 -march=native -flto=auto`
* **Threads:** 1
* **Hash size:** 1024 MB for toaster, 128 MB for Stockfish
* **Time control:** 60s + 1s

## Search Benchmark

Benchmark measured from the starting position.

| Metric                                  |    Value |
| --------------------------------------- | -------: |
| Average time to depth 20                |   19.6 s |
| Peak selective depth                    |       40 |
| Nodes searched at depth 20              |   61.6 M |
| Average NPS                             | 3.1 Mnps |
| Effective branching factor              |     2.45 |
| Average transposition table hit rate    |      80% |
| Average transposition table cutoff rate |      65% |
| Re-searches                             |      390 |
| Final evaluation                        |    +0.06 |
| Final best move                         |   `c2c4` |

## Match Testing Methodology

Performance tests were run locally using **cutechess tournaments** and analyzed with **Ordo**.

* **Total games:** 144
* **Match sets:** 3 groups of 48 games each
* **Opponents tested:** Stockfish 18 at strength level 4, UCI Elo 1700, and UCI Elo 2050
* **Opening suite:** `2moves_v1.epd`
* **Color balance:** Equal
* **Time control:** 60s + 1s

## Results

### vs Stockfish strength level 4

**Score:** 35 - 12 - 1
**Score rate:** 0.740

* White: 19 - 4 - 1
* Black: 16 - 8 - 0
* White vs Black: 27 - 20 - 1

**Elo difference:** 181.3 ± 117.5
**LOS:** 100.0%
**Draw ratio:** 2.1%

### vs Stockfish UCI Elo 1700

**Score:** 42 - 5 - 1
**Score rate:** 0.885

* White: 22 - 2 - 0
* Black: 20 - 3 - 1
* White vs Black: 25 - 22 - 1

**Elo difference:** 355.2 ± 192.3
**LOS:** 100.0%
**Draw ratio:** 2.1%

### vs Stockfish UCI Elo 2050

**Score:** 31 - 17 - 0
**Score rate:** 0.646

* White: 18 - 6 - 0
* Black: 13 - 11 - 0
* White vs Black: 29 - 19 - 0

**Elo difference:** 104.4 ± 106.7
**LOS:** 97.8%
**Draw ratio:** 0.0%

## Notes

* The current Elo values are **local estimates** from a limited sample size.


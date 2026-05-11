# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

This is `titan23`'s competitive programming library in C++. It is used for AtCoder contests (both algorithm and heuristic/AHC tracks). Libraries are header-style `.cpp` files under `titan_cpplib/` that get inlined into a single submission file via `cpp_expander.py`.

## Building and Running

There is no project-wide build system. Each file is compiled individually with g++. Typical flags:

```sh
g++ -std=c++17 -O2 -o a.out main.cpp
# With AVX2 and aggressive optimization (common in contest code):
g++ -std=c++17 -O3 -mavx2 -o a.out main.cpp
# With OpenMP (used by sa.cpp):
g++ -std=c++17 -O3 -fopenmp -o a.out main.cpp
```

The `.vscode/c_cpp_properties.json` uses `c++17` standard with MinGW on Windows.

## Code Expansion (Submission Workflow)

Library files use `#include "titan_cpplib/..."` relative includes. Before submitting to AtCoder, these must be inlined into a single file using the expander:

```sh
# Expand and copy to clipboard (default):
python3 titan_cpplib/cpp_expander.py main.cpp

# Expand to a file:
python3 titan_cpplib/cpp_expander.py main.cpp -o expanded.cpp
```

The expander handles:
- Recursively inlining `#include "titan_cpplib/..."` lines (each file included only once)
- Stripping `#pragma once`
- Stripping `#ifdef __INTELLISENSE__` blocks (used for IDE-only includes)

## Testing

Tests live in `test/`. Each subdirectory contains a test program (`main.cpp`) and optionally a test-case generator (`mk_test.py`). Typical workflow:

```sh
# Generate test input:
python3 test/dset/mk_test.py > input.txt

# Compile and run:
g++ -std=c++17 -O2 -o a.out test/dset/main.cpp
./a.out < input.txt
```

There is no automated test runner; tests are run manually.

## Repository Structure

```
titan_cpplib/          # Library source (all .cpp with #pragma once)
  ds/                  # Data structures (segment trees, heaps, union-find, wavelet matrix, etc.)
  graph/               # Graph algorithms (Dijkstra, HLD, LCT, centroid decomp, etc.)
  math/                # Math utilities (modular combinatorics, primality, factorization, etc.)
  algorithm/           # Algorithms (LIS, Mo's, doubling, random, zaatsu, etc.)
  string/              # String algorithms (Aho-Corasick, suffix automaton, Z-algorithm, etc.)
  others/              # Utilities: io.cpp, print.cpp, util.cpp
  ahc/                 # Heuristic contest frameworks (SA, beam search)
    sa/sa.cpp          # Simulated annealing engine
    beam_search/       # Beam search engine (tree-based, namespace flying_squirrel)
  cpp_expander.py      # Expander script
  format.cpp           # Boilerplate template for a new solution file
atcoder/               # ACL (AtCoder Library) headers
test/                  # Per-structure test programs and generators
```

## Library Conventions

- All library code lives in `namespace titan23`.
- Every library file starts with `#pragma once` and uses only standard includes (no `<bits/stdc++.h>` in library files themselves).
- Templates follow the ACL style for segment trees: `op`, `e`, `mapping`, `composition`, `id` as template function pointers.
- `titan_cpplib/others/print.cpp` provides debug-print overloads for containers; many library files include it.

## AHC (Heuristic) Framework

### Simulated Annealing (`titan_cpplib/ahc/sa/sa.cpp`)

`sa::sa_run<State>(TIME_LIMIT_MS, seed, verbose)` drives the search. The user implements a `State` class with:
- `State::Param` — `start_temp`, `end_temp` static member
- `State::Changed` — diff info + `TYPE_CNT`
- `State::Result` — minimal best-solution snapshot
- `init(seed)`, `modify(iter, threshold, progress)`, `rollback()`, `advance()`, `get_result()`, `print()`
- Score is **minimized**; negate for maximization problems. Do not double-negate in `get_score()`.
- Uses `sarnd` (`thread_local titan23::Random`) for fast XorShift RNG.
- See `titan_cpplib/ahc/sa/how_to_use_sa.md` for the full interface spec.

### Beam Search (`titan_cpplib/ahc/beam_search/beam_search.cpp`)

`flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF>` — tree-based beam search. The user implements:
- `Action` — transition + rollback info (keep small; many copies made)
- `State::init()`, `try_op(action, threshold)`, `apply_op(action)`, `rollback(action)`, `get_actions(actions, turn, last_action, threshold)`, `print()`, `get_state_info()`
- Uses Zobrist hashing for duplicate detection.
- Score is **minimized**; `threshold` in `try_op` is the current beam's worst score — prune if new score ≥ threshold.
- See `titan_cpplib/ahc/beam_search/how_to_use_beam_search.md` for the full interface spec.

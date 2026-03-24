# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

CMake project targeting C++23 with MinGW (primary), also supports GCC/Clang and MSVC. Developed in CLion.

```bash
# Configure and build (from project root)
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug

# Release build
cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release

# Run
./cmake-build-release/HypercubeHopfield.exe
```

## Architecture

Modern Hopfield associative memory network (Ramsauer et al., 2021) on a Boolean
hypercube (2^DIM vertices, DIM 4-16). Uses exponential energy with explicit pattern
storage and softmax-attention retrieval for exponential memory capacity.

Connectivity uses Hamming-ball neighbors: all vertices within Hamming distance `reach`
of each vertex, sorted by distance and optionally truncated by `connectivity` (0.0-1.0).
Neighbor lookup is a single XOR instruction — no adjacency storage.
See `docs/HopfieldNetwork.md`.

Key classes:
- `HopfieldNetwork<DIM>` — network core (HopfieldNetwork.h/cpp)
- `Diagnostics.h` — runner for the diagnostics suite
- `diagnostics/*.h` — header-only diagnostic classes (NoiseRecall, EnergyMonotonicity, CapacityProbe, OverlapMetrics, ParameterSweep)

## Conventions

- OpenMP is enabled for parallelism; CMakeLists.txt handles compiler-specific flags
- Source files are collected via `file(GLOB)` — new .cpp files in project root are included automatically
- Diagnostics are header-only template classes in `diagnostics/`, one per file
- Each diagnostic writes results to `diagnostics/ClassName.md` on every run
- Header guards use `#pragma once`
- All values in the pipeline are float
- Tests and diagnostics should be run in Release mode (-ffast-math can cause divergent results in Debug)

# HypercubeHopfield

A Modern Hopfield network built on a Boolean hypercube graph, achieving exponential
memory capacity through softmax-attention retrieval on structured sparse connectivity.

Licensed under the [Apache License 2.0](LICENSE).

## What is a Modern Hopfield Network?

Classical Hopfield networks store patterns via Hebbian learning into a weight matrix
and retrieve them by converging from noisy cues. Their capacity is limited to ~0.14N
patterns due to cross-talk interference.

Modern Hopfield networks (Ramsauer et al., 2021) replace the quadratic energy function
with a log-sum-exp (exponential) energy, and store patterns explicitly rather than
collapsing them into weights. Retrieval uses softmax attention — mathematically
equivalent to the transformer attention mechanism. This achieves exponential capacity
in N, far exceeding the classical limit.

## What is HypercubeHopfield?

HypercubeHopfield implements a Modern Hopfield network where the connectivity is
defined by a Boolean hypercube of dimension DIM, giving N = 2^DIM vertices (neurons).

Each vertex connects to all neighbors within a **Hamming ball** of configurable radius.
Neighbor lookup is a single XOR instruction — no adjacency list is stored. The mask
table is sorted by Hamming distance (closest first) and optionally truncated by a
`connectivity` parameter (0.0-1.0) for tunable sparsity.

At DIM=8 (N=256) with default reach=DIM/2=4 (162 connections, 63% of vertices), the
network stores 1000+ patterns with perfect recall in 2 sweeps.

The system is designed for DIM 4-16 (16 to 65536 neurons).

## Building and Running

**Requirements:** C++23 compiler (GCC 13+, Clang 16+, MSVC 19.36+), CMake 3.20+.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/HypercubeHopfield
```

OpenMP is used for parallelism where beneficial. The build system detects MinGW,
GCC/Clang, and MSVC automatically.

## Project Structure

```
HypercubeHopfield/
  HopfieldNetwork.h/cpp    Modern Hopfield network (N = 2^DIM vertices)
  Diagnostics.h            Runner for the diagnostics suite
  main.cpp                 Entry point — runs all diagnostics

  diagnostics/
    DiagnosticHelpers.h    Shared utilities (pattern gen, corruption, overlap)
    NoiseRecall.h          Single-pattern recall at varying noise levels
    EnergyMonotonicity.h   Verify energy is non-increasing across sweeps
    CapacityProbe.h        Find empirical capacity ceiling
    OverlapMetrics.h       Per-pattern overlap and cross-interference
    ParameterSweep.h       Sweep reach x beta for capacity characterization
    *.md                   Auto-generated result files from each diagnostic

  docs/
    HopfieldNetwork.md     Network architecture, connectivity, parameters
    FutureWork.md          Research directions and open questions
```

## Documentation

| Document | Covers |
|----------|--------|
| [docs/HopfieldNetwork.md](docs/HopfieldNetwork.md) | Hypercube connectivity, Hamming-ball masks, update rule, energy function, parameters |
| [docs/FutureWork.md](docs/FutureWork.md) | Capacity on sparse topologies, reservoir front-end for decorrelation |

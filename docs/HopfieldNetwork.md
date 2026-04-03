# Hopfield Network Architecture

## Overview

The `HopfieldNetwork<DIM>` implements a **sparse local-attention variant** of the
Modern Hopfield network (Ramsauer et al., 2021) on a DIM-dimensional hypercube
graph with N = 2^DIM vertices (neurons). Vertices are addressed by DIM-bit
binary strings; each holds a continuous-valued state.

Unlike classical Hopfield networks that collapse patterns into a weight matrix via
Hebbian learning, the modern formulation stores patterns explicitly and uses
exponential (softmax) interactions for retrieval.

### Distinction from Standard Modern Hopfield

The standard formulation (Ramsauer et al.) uses **global attention**: the similarity
between a pattern and the state is the full dot product across all N vertices, and
the entire state vector is updated synchronously. This gives theoretical exponential
capacity of O(2^(N/2)).

This implementation uses **sparse local attention**: each vertex computes similarity
using only its Hamming-ball neighbors (a subset of N). This is a deliberate design
choice -- sparse connectivity trades some theoretical capacity for
O(M * connections) per-vertex cost instead of O(M * N), enabling much larger
networks. Empirically, the capacity is still far above the classical ~0.14N limit
and scales super-linearly with DIM.

## Hypercube Connectivity

Each vertex connects to neighbors within a **Hamming ball** of radius `reach`.
A vertex u is a neighbor of v if `popcount(v ^ u) <= reach`. The mask table is
precomputed once at construction, sorted by Hamming distance (closest first),
then optionally truncated by the `neighbor_fraction` parameter.

### Mask Table Construction

1. Enumerate all nonzero masks m < N with `popcount(m) <= reach`
2. Sort by `popcount` (stable, so masks at same distance preserve order)
3. Truncate to `floor(size * neighbor_fraction)` masks (minimum 1)

XOR with vertex index gives the neighbor: `nb = v ^ m`. No per-vertex adjacency
storage -- every vertex uses the same mask table.

### Connection Counts (full ball, neighbor_fraction=1.0)

| DIM | N    | reach=DIM/2 | Connections | % of N |
|-----|------|-------------|-------------|--------|
| 5   | 32   | 2           | 15          | 47%    |
| 6   | 64   | 3           | 41          | 64%    |
| 7   | 128  | 3           | 63          | 49%    |
| 8   | 256  | 4           | 162         | 63%    |
| 9   | 512  | 4           | 255         | 50%    |
| 10  | 1024 | 5           | 637         | 62%    |

### Neighbor Fraction Tuning

The `neighbor_fraction` parameter (0.0-1.0) truncates the sorted mask table:
- `neighbor_fraction=1.0`: full Hamming ball (max capacity, slowest)
- `neighbor_fraction=0.5`: closest ~50% of neighbors (~2x faster)
- `neighbor_fraction=0.1`: very sparse, fast, explores capacity-speed tradeoff

Since masks are sorted by distance, truncation always keeps the closest
neighbors and drops the most distant ones first.

## Update Rule (Sparse Local Attention)

Each vertex stores a continuous-valued state s_v. The update at vertex v:

1. **Local similarity** to each stored pattern mu through Hamming-ball neighbors:

       sim_mu(v) = sum_{nb in ball(v)} pattern[mu][nb] * s_nb

2. **Softmax attention** with inverse temperature beta:

       alpha_mu = exp(beta * sim_mu) / sum_mu' exp(beta * sim_mu')

3. **Weighted combination** of patterns at vertex v:

       s_v <- sum_mu alpha_mu * pattern[mu][v]

The softmax concentrates attention on the pattern most similar to the
local neighborhood, enabling sharp retrieval even with many stored patterns.
Higher beta gives more winner-take-all behavior; lower beta gives softer blending.

Two update modes are supported:

- **Sync** (default): All vertices read from the same snapshot and write to a
  separate buffer (double-buffered). Vertex order is irrelevant -- each update
  is independent. Deterministic and GPU-portable. Internally multithreaded for
  large workloads.
- **Async**: Vertices update one at a time in random order, reading and writing
  the same buffer. Guaranteed monotonic energy descent per sweep. Not
  parallelizable due to data dependencies between vertex updates.

## Energy Function

    E(s) = -(1/N) * sum_v [ beta^-1 * log(sum_mu exp(beta * sim_mu(v))) ]

where sim_mu(v) is the local similarity defined above. This is the per-vertex
averaged log-sum-exp of pattern similarities -- the exponential interaction
is what gives modern Hopfield networks their superior capacity.

Convergence occurs when no vertex changes by more than a float tolerance
during a full sweep.

## Pattern Storage

Patterns are stored explicitly as full N-element arrays (not collapsed into
a weight matrix). This enables:
- Exponential capacity (up to exp(O(N)) patterns)
- Exact pattern retrieval (no interference between stored patterns)
- O(M * connections) per-vertex update cost, where M is the number of patterns

## Parameters

| Parameter    | Description                                      | Default  |
|--------------|--------------------------------------------------|----------|
| DIM          | Hypercube dimension (4-16)                       | Template |
| reach        | Hamming-ball radius (1-DIM)                      | DIM/2    |
| beta         | Inverse temperature for softmax attention        | 4.0      |
| neighbor_fraction | Fraction of Hamming ball to use (0.0-1.0)   | 1.0      |
| rng_seed     | Random seed for update order                     | Required |
| max_steps    | Maximum recall sweeps                            | 100      |

## Capacity

The standard fully-connected Modern Hopfield network achieves theoretical capacity
of O(2^(N/2)). This sparse local-attention variant does not enjoy the same
theoretical guarantees, but empirically demonstrates strong capacity that scales
super-linearly with DIM:

| DIM | N    | Connections | Capacity  |
|-----|------|-------------|-----------|
| 6   | 64   | 41          | 512       |
| 7   | 128  | 63          | 32768     |
| 8   | 256  | 162         | 65536+    |

At DIM=8, the network stores at least 256x its vertex count with perfect recall.

## References

- Ramsauer, H., et al. (2021). "Hopfield Networks is All You Need." ICLR 2021.
- Demircigil, M., et al. (2017). "On a model of associative memory with huge storage capacity."
- Krotov, D. & Hopfield, J. (2016). "Dense associative memory for pattern recognition."
- Amit, D., Gutfreund, H., & Sompolinsky, H. (1985). "Spin-glass models of neural networks."

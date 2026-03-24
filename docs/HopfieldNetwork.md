# Hopfield Network Architecture

## Overview

The `HopfieldNetwork<DIM>` implements a **Modern Hopfield network** (Ramsauer et al., 2021)
on a DIM-dimensional Boolean hypercube with N = 2^DIM vertices (neurons).

Unlike classical Hopfield networks that collapse patterns into a weight matrix via
Hebbian learning, the modern formulation stores patterns explicitly and uses
exponential (softmax) interactions for retrieval. This achieves exponential memory
capacity in N, far exceeding the classical ~0.14N limit.

## Hypercube Connectivity

Each vertex connects to neighbors within a **Hamming ball** of radius `reach`.
A vertex u is a neighbor of v if `popcount(v ^ u) <= reach`. The mask table is
precomputed once at construction, sorted by Hamming distance (closest first),
then optionally truncated by the `connectivity` parameter.

### Mask Table Construction

1. Enumerate all nonzero masks m < N with `popcount(m) <= reach`
2. Sort by `popcount` (stable, so masks at same distance preserve order)
3. Truncate to `floor(size * connectivity)` masks (minimum 1)

XOR with vertex index gives the neighbor: `nb = v ^ m`. No per-vertex adjacency
storage — every vertex uses the same mask table.

### Connection Counts (full ball, connectivity=1.0)

| DIM | N    | reach=DIM/2 | Connections | % of N |
|-----|------|-------------|-------------|--------|
| 5   | 32   | 2           | 15          | 47%    |
| 6   | 64   | 3           | 41          | 64%    |
| 7   | 128  | 3           | 63          | 49%    |
| 8   | 256  | 4           | 162         | 63%    |
| 9   | 512  | 4           | 255         | 50%    |
| 10  | 1024 | 5           | 637         | 62%    |

### Connectivity Tuning

The `connectivity` parameter (0.0-1.0) truncates the sorted mask table:
- `connectivity=1.0`: full Hamming ball (max capacity, slowest)
- `connectivity=0.5`: closest ~50% of neighbors (~2x faster)
- `connectivity=0.1`: very sparse, fast, explores capacity-speed tradeoff

Since masks are sorted by distance, truncation always keeps the closest
neighbors and drops the most distant ones first.

## Update Rule (Modern Hopfield / Softmax Attention)

Each vertex stores a binary spin s_v in {+1, -1}. The update at vertex v:

1. **Local similarity** to each stored pattern mu through Hamming-ball neighbors:

       sim_mu(v) = sum_{nb in ball(v)} pattern[mu][nb] * s_nb

2. **Softmax attention** with inverse temperature beta:

       alpha_mu = exp(beta * sim_mu) / sum_mu' exp(beta * sim_mu')

3. **Weighted vote** of patterns at vertex v, with sign activation:

       h_v = sum_mu alpha_mu * pattern[mu][v]
       s_v <- sign(h_v)

The softmax concentrates attention on the pattern most similar to the
local neighborhood, enabling sharp retrieval even with many stored patterns.
Higher beta gives more winner-take-all behavior; lower beta gives softer blending.

Updates are asynchronous (one vertex at a time, random order).

## Energy Function

    E(s) = -(1/N) * sum_v [ beta^-1 * log(sum_mu exp(beta * sim_mu(v))) ]

where sim_mu(v) is the local similarity defined above. This is the per-vertex
averaged log-sum-exp of pattern similarities — the exponential interaction
is what gives modern Hopfield networks their superior capacity.

Convergence occurs when no vertex changes sign during a full sweep.

## Pattern Storage

Patterns are stored explicitly as full N-element arrays (not collapsed into
a weight matrix). This enables:
- Exponential capacity (up to exp(O(N)) patterns)
- Exact pattern retrieval (no interference between stored patterns)
- O(M * connections) per-vertex update cost, where M is the number of patterns

## Parameters

| Parameter    | Description                                      | Default  |
|--------------|--------------------------------------------------|----------|
| DIM          | Hypercube dimension (5-10)                       | Template |
| reach        | Hamming-ball radius (1-DIM)                      | DIM/2    |
| beta         | Inverse temperature for softmax attention        | 4.0      |
| connectivity | Fraction of Hamming ball to use (0.0-1.0)        | 1.0      |
| rng_seed     | Random seed for update order                     | Required |
| max_steps    | Maximum recall sweeps                            | 100      |

## Capacity

Modern Hopfield networks achieve exponential capacity in N — up to exp(O(N))
patterns can be stored and retrieved reliably. At DIM=8 with reach=4 and
connectivity=1.0 (162 connections, 63% of N=256), the network stores 1000+
patterns with perfect recall. Characterizing how capacity degrades with
reduced connectivity is a key goal of this project.

## References

- Ramsauer, H., et al. (2021). "Hopfield Networks is All You Need." ICLR 2021.
- Demircigil, M., et al. (2017). "On a model of associative memory with huge storage capacity."
- Krotov, D. & Hopfield, J. (2016). "Dense associative memory for pattern recognition."
- Amit, D., Gutfreund, H., & Sompolinsky, H. (1985). "Spin-glass models of neural networks."

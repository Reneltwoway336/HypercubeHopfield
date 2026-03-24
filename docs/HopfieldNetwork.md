# Hopfield Network Architecture

## Overview

The `HopfieldNetwork<DIM>` implements a **Modern Hopfield network** (Ramsauer et al., 2021)
on a DIM-dimensional Boolean hypercube with N = 2^DIM vertices (neurons).

Unlike classical Hopfield networks that collapse patterns into a weight matrix via
Hebbian learning, the modern formulation stores patterns explicitly and uses
exponential (softmax) interactions for retrieval. This achieves exponential memory
capacity in N, far exceeding the classical ~0.14N limit.

## Hypercube Connectivity

Each vertex uses two complementary families of XOR masks for neighbor lookup,
both computed inline with no adjacency storage:

### Nearest Neighbors (DIM connections)

Single-bit-flip masks: `NearestMask(i) = 1 << i` for i=0..DIM-1

    Masks: 1, 2, 4, 8, 16, 32, ...

These are the standard hypercube edges — each flips exactly one bit, reaching
all DIM vertices at Hamming distance 1. Provides full local coverage.

### Hamming Shells (reach connections)

Cumulative-bit masks: `ShellMask(i) = (1 << (i+1)) - 1` for i=0..reach-1

    Masks: 1, 3, 7, 15, 31, 63, ...

Each mask flips the low (i+1) bits simultaneously, reaching a single vertex
at Hamming distance (i+1). Provides long-range mixing across the hypercube.

### Total Connectivity

Each vertex has **DIM + reach** connections. Note that `NearestMask(0)` and
`ShellMask(0)` both equal 1 (the same neighbor), matching the original
reservoir topology from which this design is derived.

## Update Rule (Modern Hopfield / Softmax Attention)

Each vertex stores a binary spin s_v in {+1, -1}. The update at vertex v:

1. **Local similarity** to each stored pattern mu through all neighbors:

       sim_mu(v) = sum_{nearest} pattern[mu][nb] * s_nb
                 + sum_{shells}  pattern[mu][nb] * s_nb

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
- O(M * (DIM + reach)) per-vertex update cost, where M is the number of patterns

## Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| DIM       | Hypercube dimension (5-10) | Template parameter |
| reach     | Number of Hamming shells per vertex (1-DIM) | 3 |
| beta      | Inverse temperature for softmax attention | 4.0 |
| rng_seed  | Random seed for update order | Required |
| max_steps | Maximum recall sweeps | 100 |

## Capacity

Modern Hopfield networks achieve exponential capacity in N — up to exp(O(N))
patterns can be stored and retrieved reliably. On the hypercube topology,
the effective capacity depends on the total connection count (DIM nearest +
reach shells). Characterizing this tradeoff is a key goal of this project.

## References

- Ramsauer, H., et al. (2021). "Hopfield Networks is All You Need." ICLR 2021.
- Amit, D., Gutfreund, H., & Sompolinsky, H. (1985). "Spin-glass models of neural networks."

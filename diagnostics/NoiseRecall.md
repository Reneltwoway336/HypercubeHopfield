# NoiseRecall Results

## What is NoiseRecall?

Measures the network's ability to recover a stored pattern from a noisy cue.
A single pattern is stored, then corrupted at varying noise levels (percentage
of bits flipped). The network runs Recall() and the overlap (normalized dot
product) between the recalled state and the original is measured. An overlap
of 1.0 means perfect recall; 0.0 means uncorrelated.

---

Run: DIM=8 | N=256 | reach=4 | beta=4.0 | 3-seed avg {42,1042,2042}

## Results

| Noise | Overlap | Sweeps | Result |
|-------|---------|--------|--------|
|   10% |  1.0000 |    2.0 |  PASS  |
|   20% |  1.0000 |    2.0 |  PASS  |
|   30% |  1.0000 |    2.0 |  PASS  |
|   40% |  1.0000 |    2.0 |   --   |
|   50% |  1.0000 |    2.0 |   --   |

Result: **PASS**

## Findings

- **Network recovers reliably from moderate noise.** Overlap remains high
  through 30% corruption, demonstrating robust attractor basins.

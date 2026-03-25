# NoiseRecall Results

## What is NoiseRecall?

Measures the network's ability to recover a stored pattern from a noisy cue.
A single continuous-valued pattern is stored, then corrupted with Gaussian
noise at increasing standard deviations. The network runs Recall() and the
cosine similarity between the recalled state and the original is measured.
A similarity of 1.0 means perfect recall; 0.0 means orthogonal.

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

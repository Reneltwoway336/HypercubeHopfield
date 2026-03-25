# OverlapMetrics Results

## What is OverlapMetrics?

Measures per-pattern recall quality and cross-pattern interference.
For each stored pattern, we corrupt it at 20% noise, recall, and compute:
- **Target overlap**: similarity to the intended pattern (should be ~1.0)
- **Max cross overlap**: highest similarity to any other stored pattern
  (should be low -- high values mean spurious attractor mixtures)

This diagnoses whether attractors are clean and well-separated, or whether
the network converges to spurious states that blend multiple patterns.

---

Run: DIM=8 | N=256 | reach=4 | beta=4.0 | 5 patterns | noise=20% | 3-seed avg {42,1042,2042}

## Results

| Pattern | Target Ovlp | Max Cross Ovlp | Sweeps | Result |
|---------|-------------|----------------|--------|--------|
|       0 |      1.0000 |        +0.0264 |    2.0 |  PASS  |
|       1 |      1.0000 |        +0.0723 |    2.0 |  PASS  |
|       2 |      1.0000 |        +0.0945 |    2.0 |  PASS  |
|       3 |      1.0000 |        +0.0781 |    2.0 |  PASS  |
|       4 |      1.0000 |        +0.0620 |    2.0 |  PASS  |

Result: **PASS**

## Findings

- **Attractors are clean and well-separated.** All target overlaps >= 0.90
  with low cross-pattern interference, indicating the softmax attention
  mechanism successfully isolates individual patterns.

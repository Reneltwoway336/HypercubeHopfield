# OverlapMetrics Results

## What is OverlapMetrics?

Measures per-pattern recall quality and cross-pattern interference.
5 patterns are stored, each corrupted at 20% noise, recalled, and measured:
- **Target overlap**: similarity to the intended pattern (should be ~1.0)
- **Max cross overlap**: highest similarity to any other stored pattern
  (should be low — high values mean spurious attractor mixtures)

This diagnoses whether attractors are clean and well-separated, or whether
the network converges to spurious states that blend multiple patterns.

Pass: all target overlaps >= 0.90, all cross overlaps < 0.50.

---

Run: reach=DIM/2 | beta=4.0 | 5 patterns | noise=20% | 3-seed avg {42,1042,2042}

## Summary

| DIM | N    | Connections | Avg Target | Max Cross | Result |
|-----|------|-------------|------------|-----------|--------|
| 4   | 16   | 10          | 1.0000     | +0.4583   | PASS   |
| 5   | 32   | 15          | 0.9333     | +0.3750   | FAIL   |
| 6   | 64   | 41          | 1.0000     | +0.1771   | PASS   |
| 7   | 128  | 63          | 1.0000     | +0.1302   | PASS   |
| 8   | 256  | 162         | 1.0000     | +0.0625   | PASS   |

## DIM=4 (N=16, reach=2, 10 connections)

| Pattern | Target Ovlp | Max Cross Ovlp | Sweeps | Result |
|---------|-------------|----------------|--------|--------|
|       0 |      1.0000 |        +0.2500 |    2.3 |  PASS  |
|       1 |      1.0000 |        -0.0417 |    2.3 |  PASS  |
|       2 |      1.0000 |        +0.3333 |    1.7 |  PASS  |
|       3 |      1.0000 |        +0.4583 |    2.7 |  PASS  |
|       4 |      1.0000 |        +0.2917 |    3.0 |  PASS  |

Result: **PASS** (all patterns recalled perfectly, but cross overlap is high — close to 0.50 threshold)

## DIM=5 (N=32, reach=2, 15 connections)

| Pattern | Target Ovlp | Max Cross Ovlp | Sweeps | Result |
|---------|-------------|----------------|--------|--------|
|       0 |      1.0000 |        +0.1042 |    2.0 |  PASS  |
|       1 |      1.0000 |        +0.1667 |    2.0 |  PASS  |
|       2 |      0.6667 |        +0.3750 |    2.7 |  FAIL  |
|       3 |      1.0000 |        +0.1875 |    2.0 |  PASS  |
|       4 |      1.0000 |        +0.2083 |    2.3 |  PASS  |

Result: **FAIL** (pattern 2 recalled at 0.67 — converged to a spurious state)

## DIM=6 (N=64, reach=3, 41 connections)

| Pattern | Target Ovlp | Max Cross Ovlp | Sweeps | Result |
|---------|-------------|----------------|--------|--------|
|       0 |      1.0000 |        +0.1667 |    2.0 |  PASS  |
|       1 |      1.0000 |        +0.0833 |    2.0 |  PASS  |
|       2 |      1.0000 |        +0.1042 |    2.0 |  PASS  |
|       3 |      1.0000 |        +0.1250 |    2.0 |  PASS  |
|       4 |      1.0000 |        +0.1771 |    2.0 |  PASS  |

Result: **PASS**

## DIM=7 (N=128, reach=3, 63 connections)

| Pattern | Target Ovlp | Max Cross Ovlp | Sweeps | Result |
|---------|-------------|----------------|--------|--------|
|       0 |      1.0000 |        +0.1302 |    2.0 |  PASS  |
|       1 |      1.0000 |        +0.0885 |    2.0 |  PASS  |
|       2 |      1.0000 |        +0.1094 |    2.0 |  PASS  |
|       3 |      1.0000 |        +0.0052 |    2.0 |  PASS  |
|       4 |      1.0000 |        +0.0521 |    2.0 |  PASS  |

Result: **PASS**

## DIM=8 (N=256, reach=4, 162 connections)

| Pattern | Target Ovlp | Max Cross Ovlp | Sweeps | Result |
|---------|-------------|----------------|--------|--------|
|       0 |      1.0000 |        +0.0495 |    2.0 |  PASS  |
|       1 |      1.0000 |        +0.0365 |    2.0 |  PASS  |
|       2 |      1.0000 |        +0.0625 |    2.0 |  PASS  |
|       3 |      1.0000 |        +0.0391 |    2.0 |  PASS  |
|       4 |      1.0000 |        +0.0417 |    2.0 |  PASS  |

Result: **PASS**

## Findings

- **DIM >= 6 shows perfect attractor separation.** All 5 patterns recalled at 1.0000
  target overlap with low cross-interference (max +0.18 at DIM=6, dropping to +0.06
  at DIM=8). The softmax attention cleanly isolates each pattern's basin.
- **DIM=5 fails at 5 patterns.** Pattern 2 converges to a spurious state (0.67 overlap)
  with high cross-interference (+0.375). With only 15 connections (47% of N=32), the
  local attention signal is too noisy to discriminate 5 patterns reliably. This is
  consistent with CapacityProbe showing DIM=5 capacity of only 6 patterns.
- **DIM=4 passes but is fragile.** All target overlaps are 1.0000, but cross overlaps
  reach +0.46 — just under the 0.50 threshold. A different seed could easily flip this
  to FAIL. At 10 connections in a 16-vertex network, the attractors are barely separated.
- **Cross-interference decreases with DIM.** Max cross overlap: +0.46 (DIM=4), +0.38
  (DIM=5), +0.18 (DIM=6), +0.13 (DIM=7), +0.06 (DIM=8). More connections give the
  softmax sharper discrimination between competing patterns.

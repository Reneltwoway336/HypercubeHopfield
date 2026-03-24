# NoiseRecall Results

## What is NoiseRecall?

Measures the network's ability to recover a stored pattern from a noisy cue.
A single pattern is stored, then corrupted at varying noise levels (percentage
of elements negated). The network runs Recall() and the overlap (normalized dot
product) between the recalled state and the original is measured. An overlap
of 1.0 means perfect recall; 0.0 means uncorrelated.

Pass criteria: overlap >= 0.95 at 10% noise, >= 0.90 at 20%, >= 0.80 at 30%.
40% and 50% are reported but not judged (marked --).

---

Run: reach=DIM/2 | beta=4.0 | 3-seed avg {42,1042,2042} | continuous states

## Summary

| DIM | N    | Connections | 10%    | 20%    | 30%    | 40%    | 50%    | Sweeps  | Result |
|-----|------|-------------|--------|--------|--------|--------|--------|---------|--------|
| 4   | 16   | 10          | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 1.7-2.0 | PASS  |
| 5   | 32   | 15          | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 2.0     | PASS  |
| 6   | 64   | 41          | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 2.0     | PASS  |
| 7   | 128  | 63          | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 2.0     | PASS  |
| 8   | 256  | 162         | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 2.0     | PASS  |

## Per-DIM Results

Results are identical across all DIMs — perfect 1.0000 overlap at every noise
level in 2 sweeps (1.7 at DIM=4/10% where fewer elements need correcting).

| Noise | Overlap | Sweeps | Result |
|-------|---------|--------|--------|
|   10% |  1.0000 |    2.0 |  PASS  |
|   20% |  1.0000 |    2.0 |  PASS  |
|   30% |  1.0000 |    2.0 |  PASS  |
|   40% |  1.0000 |    2.0 |   --   |
|   50% |  1.0000 |    2.0 |   --   |

## Findings

- **Perfect recall at all noise levels, all DIMs — unchanged from binary formulation.**
  The switch to continuous states did not affect single-pattern recall. With only 1
  stored pattern, the softmax attention has no competing patterns, so the weighted
  combination converges to the stored pattern exactly.
- **Single-pattern recall is trivially easy.** This test validates basic correctness
  rather than probing capacity limits (see CapacityProbe for that).
- **Convergence is uniformly fast.** 2 sweeps across all DIMs and noise levels.
  The continuous update converges as quickly as the binary sign activation did
  for single-pattern recall.

# CapacityProbe Results

## What is CapacityProbe?

Measures the network's empirical memory capacity: the maximum number of
patterns that can be stored and reliably recalled from 20% noise. Capacity
is defined as the largest pattern count where mean overlap >= 0.90.

Classical Hopfield capacity is ~0.14N. Modern Hopfield networks with
exponential energy functions theoretically achieve exponential capacity
in N. This diagnostic measures the empirical limit on the sparse hypercube
topology with dual-mask connectivity (DIM nearest + reach shells).

---

Run: DIM=8 | N=256 | reach=4 | beta=4.0 | noise=20% | 3-seed avg {42,1042,2042}

## Results

| Count | Mean Ovlp | Min Ovlp | Sweeps | Status |
|-------|-----------|----------|--------|--------|
|     1 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|     2 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|     3 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|     4 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|     6 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|     8 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|    12 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|    16 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|    24 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|    32 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|    48 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|    64 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|   128 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|   256 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|   512 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|  1024 |    1.0000 |   1.0000 |    2.0 |  PASS  |

Capacity (overlap >= 90%): 1024 patterns (400.00% of N=256)
Result: **PASS**

## Findings

- **Empirical capacity: 1024 patterns.** This exceeds the classical Hopfield limit of ~0.14N (= 35),
  confirming the benefit of the modern exponential energy function.

# CapacityProbe Results

## What is CapacityProbe?

Measures the network's empirical memory capacity: the maximum number of
patterns that can be stored and reliably recalled from 20% noise. Capacity
is defined as the largest pattern count where mean overlap >= 0.90.

Classical Hopfield capacity is ~0.14N. Modern Hopfield networks with
exponential energy functions theoretically achieve exponential capacity
in N. This diagnostic measures the empirical limit on the sparse hypercube
topology with Hamming-ball connectivity.

At high pattern counts, a random sample of 64 patterns is tested per seed
rather than all M (marked with * in the table).

---

Run: DIM=8 | N=256 | reach=4 | beta=4.0 | noise=20% | ceiling=8192 | 3-seed avg {42,1042,2042}

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
|*  128 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|*  256 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|*  512 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|* 1024 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|* 2048 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|* 4096 |    1.0000 |   1.0000 |    2.0 |  PASS  |
|* 8192 |    1.0000 |   1.0000 |    2.0 |  PASS  |

Capacity (overlap >= 90%): 8192+ patterns (3200.00% of N=256)
(ceiling reached -- increase max_patterns to probe higher)
Result: **PASS**

## Findings

- **Capacity: 8192+ patterns (ceiling reached).** The network showed perfect
  recall at all tested counts. The true capacity is higher -- increase
  max_patterns to probe further.

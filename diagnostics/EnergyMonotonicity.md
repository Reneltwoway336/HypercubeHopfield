# EnergyMonotonicity Results

## What is EnergyMonotonicity?

Verifies that the network energy E(s) is non-increasing across asynchronous
update sweeps during recall. This is a fundamental guarantee of Hopfield
networks: each update step should either decrease energy or leave it unchanged.
An energy increase indicates a bug in the update rule or energy function.

The modern Hopfield energy is:

    E(s) = -(1/N) * sum_v [ beta^-1 * log(sum_mu exp(beta * sim_mu(v))) ]

---

Run: DIM=8 | N=256 | reach=4 | beta=4.0 | 3 stored patterns | 3-seed {42,1042,2042}

## Results


  Seed 42:
| Sweep |     Energy |    Delta | Result |
|-------|------------|----------|--------|
|     0 |   -13.8050 |       -- |   --   |
|     1 |  -162.0000 | -148.19499 |  PASS  |
|     2 |  -162.0000 | +0.00000 |  PASS  |
|     3 |  -162.0000 | +0.00000 |  PASS  |
|     4 |  -162.0000 | +0.00000 |  PASS  |

(converged at sweep 4)

  Seed 1042:
| Sweep |     Energy |    Delta | Result |
|-------|------------|----------|--------|
|     0 |    -5.8417 |       -- |   --   |
|     1 |  -155.6719 | -149.83020 |  PASS  |
|     2 |  -162.0000 | -6.32812 |  PASS  |
|     3 |  -162.0000 | +0.00000 |  PASS  |
|     4 |  -162.0000 | +0.00000 |  PASS  |
|     5 |  -162.0000 | +0.00000 |  PASS  |

(converged at sweep 5)

  Seed 2042:
| Sweep |     Energy |    Delta | Result |
|-------|------------|----------|--------|
|     0 |    -7.5944 |       -- |   --   |
|     1 |  -162.0000 | -154.40558 |  PASS  |
|     2 |  -162.0000 | +0.00000 |  PASS  |
|     3 |  -162.0000 | +0.00000 |  PASS  |
|     4 |  -162.0000 | +0.00000 |  PASS  |

(converged at sweep 4)

Result: **PASS**

## Findings

- **Energy is strictly non-increasing across all seeds.** All consecutive
  sweep pairs show delta <= 0 (within float tolerance), confirming the
  update rule and energy function are consistent.

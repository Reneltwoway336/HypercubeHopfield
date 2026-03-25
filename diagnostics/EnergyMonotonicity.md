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


### Seed 42

| Sweep |     Energy |    Delta | Result |
|-------|------------|----------|--------|
|     0 |    -6.0203 |       -- |   --   |
|     1 |   -57.4182 | -51.39793 |  PASS  |
|     2 |   -57.4183 | -0.00010 |  PASS  |
|     3 |   -57.4183 | +0.00000 |  PASS  |
|     4 |   -57.4183 | +0.00000 |  PASS  |
|     5 |   -57.4183 | +0.00000 |  PASS  |

(converged at sweep 5)

### Seed 1042

| Sweep |     Energy |    Delta | Result |
|-------|------------|----------|--------|
|     0 |    -2.3500 |       -- |   --   |
|     1 |   -50.5718 | -48.22174 |  PASS  |
|     2 |   -58.4060 | -7.83421 |  PASS  |
|     3 |   -58.4060 | +0.00000 |  PASS  |
|     4 |   -58.4060 | +0.00000 |  PASS  |
|     5 |   -58.4060 | +0.00000 |  PASS  |

(converged at sweep 5)

### Seed 2042

| Sweep |     Energy |    Delta | Result |
|-------|------------|----------|--------|
|     0 |    -3.0902 |       -- |   --   |
|     1 |   -50.3664 | -47.27620 |  PASS  |
|     2 |   -50.3664 | +0.00000 |  PASS  |
|     3 |   -50.3664 | +0.00000 |  PASS  |
|     4 |   -50.3664 | +0.00000 |  PASS  |
|     5 |   -50.3664 | -0.00000 |  PASS  |
|     6 |   -50.3664 | +0.00000 |  PASS  |
|     7 |   -50.3664 | +0.00000 |  PASS  |
|     8 |   -50.3664 | +0.00000 |  PASS  |
|     9 |   -50.3664 | -0.00000 |  PASS  |
|    10 |   -50.3664 | -0.00001 |  PASS  |
|    11 |   -50.3664 | +0.00000 |  PASS  |
|    12 |   -50.3664 | +0.00001 |  PASS  |
|    13 |   -50.3664 | -0.00001 |  PASS  |
|    14 |   -50.3664 | +0.00000 |  PASS  |
|    15 |   -50.3664 | +0.00000 |  PASS  |
|    16 |   -50.3664 | +0.00000 |  PASS  |

(converged at sweep 16)

Result: **PASS**

## Findings

- **Energy is strictly non-increasing across all seeds.** All consecutive
  sweep pairs show delta <= 0 (within float tolerance), confirming the
  update rule and energy function are consistent.

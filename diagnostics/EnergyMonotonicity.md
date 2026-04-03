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
|     1 |   -57.4182 | -51.39794 |  PASS  |
|     2 |   -57.4183 | -0.00009 |  PASS  |
|     3 |   -57.4183 | +0.00000 |  PASS  |
|     4 |   -57.4183 | +0.00000 |  PASS  |
|     5 |   -57.4183 | +0.00000 |  PASS  |

(converged at sweep 5)

### Seed 1042

| Sweep |     Energy |    Delta | Result |
|-------|------------|----------|--------|
|     0 |    -2.3500 |       -- |   --   |
|     1 |   -50.5718 | -48.22175 |  PASS  |
|     2 |   -58.4060 | -7.83420 |  PASS  |
|     3 |   -58.4060 | +0.00000 |  PASS  |
|     4 |   -58.4060 | +0.00000 |  PASS  |
|     5 |   -58.4060 | +0.00000 |  PASS  |

(converged at sweep 5)

### Seed 2042

| Sweep |     Energy |    Delta | Result |
|-------|------------|----------|--------|
|     0 |    -3.0902 |       -- |   --   |
|     1 |   -50.3664 | -47.27618 |  PASS  |
|     2 |   -50.3664 | +0.00000 |  PASS  |
|     3 |   -50.3664 | +0.00000 |  PASS  |
|     4 |   -50.3664 | +0.00000 |  PASS  |

(converged at sweep 4)

Result: **PASS**

## Findings

- **Energy is strictly non-increasing across all seeds.** All consecutive
  sweep pairs show delta <= 0 (within float tolerance), confirming the
  update rule and energy function are consistent.

# Autoassociative Memory: Sensor Fault Recovery

## Scenario

Imagine a facility monitored by 256 sensors -- temperature, pressure, vibration,
flow rate -- spread across a plant floor. The network has learned 20 known-good
operating profiles.

When sensors fail or return garbage, the network uses the remaining good readings
to reconstruct what the faulty sensors should be reporting. This is
content-addressable error correction: the partial input is enough to recall the
whole.

| Parameter        | Value |
|------------------|-------|
| DIM              | 8     |
| Sensors (N)      | 256   |
| Stored profiles  | 20    |

---

## Test 1: Gaussian Noise on All Sensors

Every sensor drifts from its true value by a random amount. Noise level is the
standard deviation of the drift relative to signal values in [-1, 1]. Higher
noise means more corruption.

| Noise | Similarity Before | Similarity After | Sweeps | Result    |
|------:|------------------:|-----------------:|-------:|-----------|
|   0.5 |            0.7389 |           1.0000 |      2 | RECOVERED |
|   1.0 |            0.4506 |           1.0000 |      2 | RECOVERED |
|   1.5 |            0.3124 |           1.0000 |      3 | RECOVERED |
|   2.0 |            0.2662 |           1.0000 |      3 | RECOVERED |
|   3.0 |            0.1666 |           1.0000 |      3 | RECOVERED |
|   5.0 |            0.0514 |           0.0397 |      3 | FAILED    |

The network recovers perfectly up to noise = 3.0. At that level, the input
similarity to the true profile is already very low (0.17) -- yet the attractor
basin pulls it back completely. Beyond that threshold, the corrupted state lands
in the wrong basin and converges to a different attractor.

---

## Test 2: Sensor Dropout (Dead Sensors Reporting Zero)

A percentage of sensors go completely dead, reporting 0.0 instead of their true
values. The network must infer the missing readings from the survivors alone.

| Dead % | Dead Sensors | Similarity Before | Similarity After | Sweeps | Result    |
|-------:|-------------:|------------------:|-----------------:|-------:|-----------|
|    10% |           25 |            0.9562 |           1.0000 |      2 | RECOVERED |
|    30% |           76 |            0.8129 |           1.0000 |      2 | RECOVERED |
|    50% |          128 |            0.6933 |           1.0000 |      2 | RECOVERED |
|    70% |          179 |            0.5396 |           1.0000 |      2 | RECOVERED |
|    90% |          230 |            0.3184 |           1.0000 |      3 | RECOVERED |

Even with 90% of sensors dead, the network reconstructs the full profile
perfectly. The surviving sensors provide enough context to identify which stored
profile matches, and the Hopfield dynamics fill in every missing value.

---

## Takeaway

This is the practical value of associative memory: partial information is
sufficient for complete reconstruction, as long as the input falls within the
correct attractor basin.

With 20 stored profiles on a 256-vertex hypercube, the network tolerates extreme
corruption (noise sigma up to 3.0, well beyond the [-1, 1] signal amplitude) and
extreme data loss (90% of sensors dead) while still recovering the original
profile in 2-3 sweeps.

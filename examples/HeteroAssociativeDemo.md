# Heteroassociative Memory: Diagnostic Lookup

## Scenario

Unlike autoassociative memory (which recovers a corrupted version of what it
already knows), heteroassociative memory maps one representation to a different
one: given input A, recall the associated output B.

Here, an industrial system has learned 15 condition-to-diagnosis mappings. The
256-vertex hypercube is split in half:

- **Vertices 0-127:** sensor measurements (input)
- **Vertices 128-255:** diagnostic response (output)

The network stores composite [input | output] patterns. At query time, the input
half is filled and the output half is zeroed. The network's dynamics complete the
association.

| Parameter      | Value |
|----------------|-------|
| DIM            | 8     |
| Vertices (N)   | 256   |
| Input half     | 128   |
| Output half    | 128   |
| Stored pairs   | 15    |
| Update mode    | Sync  |

---

## Test 1: Clean Input Recall

Each stored input is presented exactly, with the output half zeroed. Can the
network fill in the correct diagnosis?

| Pair | Output Similarity | Input Similarity | Sweeps | Result |
|-----:|------------------:|-----------------:|-------:|--------|
|    1 |            1.0000 |           1.0000 |      2 | OK     |
|    2 |            1.0000 |           1.0000 |      2 | OK     |
|    3 |            1.0000 |           1.0000 |      2 | OK     |
|    4 |            1.0000 |           1.0000 |      2 | OK     |
|    5 |            1.0000 |           1.0000 |      2 | OK     |
|    6 |            1.0000 |           1.0000 |      2 | OK     |
|    7 |            1.0000 |           1.0000 |      2 | OK     |
|    8 |            1.0000 |           1.0000 |      2 | OK     |
|    9 |            1.0000 |           1.0000 |      2 | OK     |
|   10 |            1.0000 |           1.0000 |      2 | OK     |
|   11 |            1.0000 |           1.0000 |      2 | OK     |
|   12 |            1.0000 |           1.0000 |      2 | OK     |
|   13 |            1.0000 |           1.0000 |      2 | OK     |
|   14 |            1.0000 |           1.0000 |      2 | OK     |
|   15 |            1.0000 |           1.0000 |      2 | OK     |

**Result: 15 of 15 pairs recalled correctly.**

Every stored mapping was recovered perfectly. The network sees the input half,
identifies which composite pattern it belongs to, and fills in the associated
output -- even though the output was entirely absent from the query.

---

## Test 2: Noisy Input Recall

Real sensors drift. Gaussian noise is added to the input half of pair #1 before
querying. The output half starts at zero as before. How much input noise can the
network tolerate and still produce the correct diagnosis?

| Noise | Output Similarity | Result |
|------:|------------------:|--------|
|   0.3 |            1.0000 | OK     |
|   0.6 |            1.0000 | OK     |
|   1.0 |            1.0000 | OK     |
|   1.5 |            1.0000 | OK     |
|   2.0 |            1.0000 | OK     |
|   3.0 |            1.0000 | OK     |

The correct diagnosis is retrieved even with noise up to 3.0 on the input
sensors. The network does not need a perfect query -- it finds the nearest stored
association and completes it.

---

## Test 3: Ambiguous Input (Blend of Two Conditions)

What happens when the input is a mix of two known conditions? The input is a
weighted blend of pairs #1 and #2. As the balance shifts, the network should snap
to the dominant association -- not produce a blended output.

| Blend             | Sim to #1 | Sim to #2 | Winner |
|-------------------|----------:|----------:|--------|
| 90% #1 / 10% #2  |    1.0000 |    0.0568 | #1     |
| 70% #1 / 30% #2  |    1.0000 |    0.0568 | #1     |
| 50% #1 / 50% #2  |    1.0000 |    0.0568 | #1     |
| 30% #1 / 70% #2  |    0.0568 |    1.0000 | #2     |
| 10% #1 / 90% #2  |    0.0568 |    1.0000 | #2     |

The network exhibits winner-take-all behavior: at every blend ratio it commits
fully to one diagnosis rather than producing a meaningless average. Near the
crossover point, which attractor wins depends on subtle pattern geometry -- but
the network always decides, never compromises.

---

## Takeaway

Heteroassociative recall turns the Hopfield network into a learned lookup table
that generalizes gracefully. With 15 stored input-output mappings on a
256-vertex hypercube:

- **Clean inputs** produce perfect output recall (15/15).
- **Noisy inputs** (up to 3.0 sigma) still produce the correct diagnosis.
- **Ambiguous inputs** trigger a sharp winner-take-all decision at the crossover
  point, rather than blending outputs.

The network does not interpolate -- it classifies and commits.

# Future Work

## 1. Modern Hopfield Capacity on Sparse Topologies

### Background

Classical Hopfield networks with quadratic energy E = -1/2 x^T W x have a capacity
ceiling of ~0.138N patterns due to cross-talk interference between stored patterns.
Modern Hopfield networks (Ramsauer et al., 2021; Demircigil et al., 2017) replace
this with a log-sum-exp energy:

    E = -log sum_i exp(xi_i^T x) + terms

This creates exponentially deep, narrow basins around each pattern, yielding a
theoretical capacity of O(2^(N/2)) — exponential in the number of neurons.

The exponential sharpness provides such large inter-basin margins that sparse
connectivity (which degrades classical networks severely) becomes a secondary
concern. The bottleneck shifts from interference to whether the update dynamics
can find the correct basin — a less connectivity-sensitive problem.

### Connection to Transformers

Ramsauer et al. showed that the modern Hopfield retrieval rule is mathematically
equivalent to the transformer attention mechanism: softmax over key-query dot
products is one step of retrieval in an exponential Hopfield network. This provides
a principled theoretical framework for understanding why attention works.

### Open Questions for This Project

- **Empirical capacity vs. theory on the hypercube.** The theoretical 2^(N/2) bound
  assumes patterns in general position. How does the dual-mask hypercube topology
  (DIM nearest + reach shells) affect the empirical capacity? The CapacityProbe and
  ParameterSweep diagnostics are designed to characterize this.
- **Reach-beta interaction.** How does the optimal inverse temperature (beta) depend
  on connectivity (reach)? Sparser networks may need lower beta to smooth noisy
  local similarity signals.
- **Continuous vs. binary states.** The exponential capacity result holds most cleanly
  for continuous states. Our network uses binary (+1/-1) with sign activation.
  Exploring continuous-state retrieval (removing the sign threshold) could unlock
  higher capacity.

### Caveats

- Exponential capacity is a theoretical upper bound assuming patterns in general
  position (not too correlated). Highly structured or correlated pattern sets will
  have lower effective capacity.
- As capacity is approached, basins of attraction become extremely narrow, reducing
  noise tolerance in the query. The CapacityProbe diagnostic measures this tradeoff.

---

## 2. Reservoir Front-End for Pattern Decorrelation

### Concept

Use an untrained reservoir computing layer (without readout) as a nonlinear
decorrelating front-end to the Hopfield network. The reservoir expands inputs into
a high-dimensional dynamical state space, pushing correlated patterns closer to
the general-position assumption required for exponential capacity.

### Why It Should Help

- **Decorrelation.** A random recurrent reservoir acts as a nonlinear random
  projection. Inputs that are close together in the original space get scattered
  more widely in the reservoir state space due to nonlinear mixing.
- **Dimensionality expansion.** If inputs are N-dimensional but the reservoir has
  M >> N neurons, patterns are stored in a larger space where random vectors are
  nearly orthogonal. Capacity could scale as 2^(M/2) instead of 2^(N/2).

### Challenges

- **Two-stage retrieval.** Stored patterns are reservoir states, but queries arrive
  in input space. Queries must be run through the same reservoir dynamics before
  Hopfield retrieval. This works if the reservoir is deterministic and the query
  is a noisy version of a previously seen input.
- **Sensitivity tradeoff.** Reservoir dynamics that decorrelate similar patterns
  also amplify noise in corrupted queries, especially near the edge of chaos. There
  is likely an optimal spectral radius regime that decorrelates patterns without
  destroying basin-of-attraction structure.
- **Chaos vs. stability.** The reservoir's spectral radius must be tuned to balance
  decorrelation gain against retrieval robustness — an interesting parameter to
  characterize.

### Related Work

- **Random projections for Hopfield networks.** Johnson-Lindenstrauss-type
  embeddings preserve distances while improving orthogonality, but these are
  static linear maps, not dynamical systems.
- **Reservoir + attractor models.** Dominey et al. studied recurrent dynamics
  feeding into associative memory in prefrontal cortex models, but focused on
  sequence learning rather than capacity.
- **Echo state separation property.** Jaeger's work on echo state networks
  formally characterizes how different input histories produce different reservoir
  states — closely related to decorrelation.
- **Kernel methods and Hopfield networks.** Patterns are implicitly mapped to a
  higher-dimensional feature space before storage. The reservoir acts as a
  specific dynamical, temporal kernel.

### Research Direction

The specific combination — an untrained random recurrent network as a fixed
nonlinear front-end to a modern Hopfield network, analyzed for capacity scaling —
appears to be novel. Key questions:

1. How does effective capacity scale with reservoir size, spectral radius, and
   input correlation structure?
2. Is there a principled way to set reservoir hyperparameters to maximize capacity
   gain while preserving retrieval robustness?
3. Can random matrix theory (for the reservoir) be combined with existing capacity
   proofs (for the Hopfield side) to derive analytical bounds?

This project's hypercube reservoir (from HypercubeReservoirComputer) is a natural
candidate for the front-end, sharing the same substrate topology.

---

## References

- Ramsauer, H., et al. (2021). "Hopfield Networks is All You Need." ICLR 2021.
- Demircigil, M., et al. (2017). "On a model of associative memory with huge
  storage capacity." Journal of Statistical Physics.
- Krotov, D. & Hopfield, J. (2016). "Dense associative memory for pattern
  recognition." NeurIPS 2016.
- Amit, D., Gutfreund, H., & Sompolinsky, H. (1985). "Spin-glass models of
  neural networks." Physical Review A.

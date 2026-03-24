#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <random>

/// Modern Hopfield network (Ramsauer et al., 2021) on a DIM-dimensional Boolean
/// hypercube (N = 2^DIM vertices).
///
/// Uses an exponential energy function with explicit pattern storage and
/// softmax-attention retrieval, achieving exponential memory capacity in N.
///
/// Each vertex stores a binary spin (+1/-1). The update rule computes a
/// softmax-weighted combination of stored patterns using local similarities
/// through dual-mask hypercube neighbors, then applies sign activation.
///
/// Connectivity uses two complementary mask families per vertex:
///   - Nearest neighbors: single-bit-flip masks (1<<i) for i=0..DIM-1,
///     giving DIM connections at Hamming distance 1 (standard hypercube edges).
///   - Hamming shells: cumulative-bit masks ((1<<(i+1))-1) for i=0..reach-1,
///     reaching progressively more distant vertices (distance 1..reach).
///
/// Total connections per vertex = DIM + reach. The nearest-neighbor masks
/// provide full local coverage; the Hamming shells add long-range mixing.
/// All masks are computed inline -- no adjacency storage.
///
/// Call StorePattern() to add patterns (stored explicitly, not collapsed into
/// weights), then Recall() to converge from a noisy or partial cue.
template <size_t DIM>
class HopfieldNetwork
{
    static_assert(DIM >= 5 && DIM <= 10, "DIM must be in 5 <= DIM <= 10");

    static constexpr size_t N = 1ULL << DIM;
    static constexpr size_t MASK = N - 1;

public:
    static constexpr size_t dim = DIM;
    static constexpr size_t num_vertices = N;

    /// Nearest-neighbor mask: single bit flip at position i.
    /// Masks: 1, 2, 4, 8, 16, ...  (Hamming distance 1, standard hypercube edges)
    static constexpr uint32_t NearestMask(size_t i) { return 1u << i; }

    /// Hamming-shell mask: cumulative low-bit flip.
    /// Masks: 1, 3, 7, 15, 31, ...  (Hamming distance 1, 2, 3, ...)
    /// Provides long-range mixing across the hypercube.
    static constexpr uint32_t ShellMask(size_t i) { return (1u << (i + 1)) - 1; }

    /// @param rng_seed Random seed for update order permutations.
    /// @param reach Number of Hamming shells per vertex (1 to DIM). Added on
    ///              top of DIM nearest-neighbor connections for long-range mixing.
    /// @param beta Inverse temperature for softmax attention. Higher values
    ///             give sharper retrieval (closer to winner-take-all).
    static std::unique_ptr<HopfieldNetwork> Create(uint64_t rng_seed,
                                                   size_t reach = 3,
                                                   float beta = 4.0f)
    {
        return std::unique_ptr<HopfieldNetwork>(new HopfieldNetwork(rng_seed, reach, beta));
    }

    HopfieldNetwork(const HopfieldNetwork&) = delete;
    HopfieldNetwork& operator=(const HopfieldNetwork&) = delete;

    /// @brief Store a pattern into the network (explicit storage, not Hebbian).
    /// @param pattern Array of N floats, each +1 or -1.
    void StorePattern(const float* pattern);

    /// @brief Run asynchronous updates until convergence or max_steps.
    /// @param state In/out: N-element spin array (+1/-1). Modified in place.
    /// @param max_steps Maximum update sweeps before declaring non-convergence.
    /// @return Number of sweeps taken (< max_steps means converged).
    size_t Recall(float* state, size_t max_steps = 100);

    /// @brief Compute modern Hopfield energy for the given state.
    /// E(s) = -(1/N) * sum_v lse(beta, sim_v) where sim_v are per-vertex
    /// pattern similarities through nearest + shell neighbors.
    [[nodiscard]] float Energy(const float* state) const;

    /// @brief Number of patterns currently stored.
    [[nodiscard]] size_t NumPatterns() const { return num_patterns_; }

    /// @brief Number of Hamming shells (long-range connections per vertex).
    [[nodiscard]] size_t Reach() const { return reach_; }

    /// @brief Total connections per vertex (DIM nearest + reach shells).
    [[nodiscard]] size_t NumConnections() const { return DIM + reach_; }

    /// @brief Inverse temperature parameter.
    [[nodiscard]] float Beta() const { return beta_; }

    /// @brief Clear all stored patterns.
    void Clear();

    [[nodiscard]] const float* State() const { return vtx_state_; }

private:
    explicit HopfieldNetwork(uint64_t rng_seed, size_t reach, float beta);

    size_t reach_;           // number of Hamming shells per vertex (1..DIM)
    float beta_;             // inverse temperature for softmax attention
    size_t num_patterns_ = 0;
    std::mt19937_64 rng_;    // persists across Recall() calls for varied orderings

    alignas(64) float vtx_state_[N]{};
    std::vector<float> patterns_;  // flat [num_patterns_ * N], explicit pattern storage
    std::vector<float> sim_buf_;   // reusable similarity buffer [num_patterns_]

    void Initialize();
    void UpdateVertex(size_t v);
};

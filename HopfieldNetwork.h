#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
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
/// through Hamming-ball neighbors, then applies sign activation.
///
/// Connectivity: each vertex connects to neighbors within Hamming distance
/// `reach` (the Hamming ball of radius `reach`). The mask table is sorted by
/// Hamming distance (closest first), then truncated to `connectivity` percent
/// of the full ball. This gives a tunable sparsity knob:
///   - connectivity=1.0: full Hamming ball (max capacity, slowest)
///   - connectivity=0.5: half the connections, ~2x faster
///   - connectivity=0.1: very sparse, fast, reduced capacity
///
/// The neighbor mask table is precomputed once at construction. XOR with
/// vertex index gives the neighbor: nb = v ^ m. No per-vertex adjacency storage.
///
/// Call StorePattern() to add patterns (stored explicitly, not collapsed into
/// weights), then Recall() to converge from a noisy or partial cue.
template <size_t DIM>
class HopfieldNetwork
{
    static_assert(DIM >= 4 && DIM <= 16, "DIM must be in 4 <= DIM <= 16");

    static constexpr size_t N = 1ULL << DIM;

public:
    static constexpr size_t dim = DIM;
    static constexpr size_t num_vertices = N;

    /// @param rng_seed Random seed for update order permutations.
    /// @param reach Max Hamming distance for neighbors (1 to DIM). Controls
    ///              the radius of the Hamming ball around each vertex.
    ///              Default DIM/2 gives ~50-63% connectivity.
    /// @param beta Inverse temperature for softmax attention. Higher values
    ///             give sharper retrieval (closer to winner-take-all).
    /// @param connectivity Fraction of the Hamming ball to use (0.0-1.0).
    ///                     Masks are sorted by distance (closest first) then
    ///                     truncated. Default 1.0 uses the full ball.
    static std::unique_ptr<HopfieldNetwork> Create(uint64_t rng_seed,
                                                   size_t reach = DIM / 2,
                                                   float beta = 4.0f,
                                                   float connectivity = 1.0f)
    {
        return std::unique_ptr<HopfieldNetwork>(new HopfieldNetwork(rng_seed, reach, beta, connectivity));
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
    /// pattern similarities through Hamming-ball neighbors.
    [[nodiscard]] float Energy(const float* state) const;

    /// @brief Number of patterns currently stored.
    [[nodiscard]] size_t NumPatterns() const { return num_patterns_; }

    /// @brief Hamming-ball radius (max Hamming distance for neighbors).
    [[nodiscard]] size_t Reach() const { return reach_; }

    /// @brief Number of connections per vertex (after connectivity truncation).
    [[nodiscard]] size_t NumConnections() const { return conn_masks_.size(); }

    /// @brief Connectivity fraction (0.0-1.0).
    [[nodiscard]] float Connectivity() const { return connectivity_; }

    /// @brief Inverse temperature parameter.
    [[nodiscard]] float Beta() const { return beta_; }

    /// @brief Clear all stored patterns.
    void Clear();

    [[nodiscard]] const float* State() const { return vtx_state_; }

private:
    explicit HopfieldNetwork(uint64_t rng_seed, size_t reach, float beta, float connectivity);

    size_t reach_;           // Hamming-ball radius (1..DIM)
    float beta_;             // inverse temperature for softmax attention
    float connectivity_;     // fraction of Hamming ball used (0.0-1.0)
    size_t num_patterns_ = 0;
    std::mt19937_64 rng_;    // persists across Recall() calls for varied orderings

    alignas(64) float vtx_state_[N]{};
    std::vector<float> patterns_;    // row-major [num_patterns_ * N] for StorePattern
    mutable std::vector<float> patterns_t_;  // col-major [N * num_patterns_] for fast Recall
    mutable bool patterns_dirty_ = true;     // true when patterns_t_ needs rebuild
    std::vector<float> sim_buf_;     // reusable similarity buffer [num_patterns_]
    std::vector<uint32_t> conn_masks_;  // precomputed neighbor masks: popcount(m) <= reach_

    void Initialize();
    void BuildMaskTable();
    void EnsureTransposed() const;
    void UpdateVertex(size_t v);
};

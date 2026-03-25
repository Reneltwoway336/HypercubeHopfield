#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <random>

/// Modern Hopfield associative memory on a hypercube graph.
///
/// Implements the exponential-energy Hopfield network of Ramsauer et al. (2021)
/// on a DIM-dimensional hypercube with N = 2^DIM vertices. Vertices are
/// addressed by DIM-bit binary strings; each vertex holds a continuous-valued
/// state. Patterns are stored explicitly (not collapsed into weights) and
/// retrieved via softmax attention, giving exponential memory capacity in N.
///
/// Architecture:
///   - Each vertex holds a continuous-valued state in the range produced by
///     softmax-weighted voting over stored patterns.
///   - Neighbors are all vertices within Hamming distance `reach` (the Hamming
///     ball). Neighbor lookup is a single XOR -- no adjacency storage required.
///   - Asynchronous updates: each sweep visits every vertex in random order,
///     replacing its state with the softmax-attention output over its neighbors.
///   - Convergence is detected when no vertex changes by more than 1e-6 in a
///     full sweep.
///
/// Usage:
///   auto net = HopfieldNetwork<8>::Create(seed);
///   net->StorePattern(pattern1);
///   net->StorePattern(pattern2);
///   net->Recall(noisy_cue);  // noisy_cue converges to nearest stored pattern
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
    /// @param pattern Array of N floats (continuous-valued).
    void StorePattern(const float* pattern);

    /// @brief Run asynchronous updates until convergence or max_steps.
    /// @param state In/out: N-element state array (continuous-valued). Modified in place.
    /// @param max_steps Maximum update sweeps before declaring non-convergence.
    /// @return 0 if no patterns stored (input unchanged), 1..max_steps-1 if
    ///         converged, max_steps if not converged.
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

    /// @brief Pointer to the internal N-element vertex state array.
    /// Valid after Recall(); reflects the converged (or last) state.
    [[nodiscard]] const float* State() const { return vtx_state_; }

private:
    HopfieldNetwork(uint64_t rng_seed, size_t reach, float beta, float connectivity);

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
    std::vector<size_t> perm_;       // reusable permutation array for Recall [N]
    std::vector<uint32_t> conn_masks_;  // precomputed neighbor masks: popcount(m) <= reach_

    void Initialize();
    void BuildMaskTable();
    void EnsureTransposed() const;
    void UpdateVertex(size_t v);
};

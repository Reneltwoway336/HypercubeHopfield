// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 David Liptak

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include <random>

class ThreadPool; // defined in ThreadPool.h (included by .cpp only)

/// Update mode for Recall().
enum class UpdateMode : uint8_t
{
    Async, ///< Sequential random-order updates. Guaranteed energy descent.
    Sync   ///< Simultaneous double-buffered updates. Deterministic, GPU-portable.
};

/// Result returned by Recall().
struct RecallResult
{
    size_t steps; ///< Number of update sweeps performed (0 if no patterns stored).
    bool converged; ///< True if the state stabilized within tolerance.
};

/// Type-erased interface for HopfieldNetwork.
///
/// Allows runtime DIM selection -- SDK bindings (Python, etc.) hold a
/// unique_ptr<IHopfieldNetwork> and dispatch through virtual calls.
/// All buffer-accepting methods use std::span with runtime size validation.
///
/// @note Not thread-safe. A single instance must not be accessed concurrently.
///       Create separate instances for concurrent use.
class IHopfieldNetwork
{
public:
    virtual ~IHopfieldNetwork() = default;
    IHopfieldNetwork() = default;
    IHopfieldNetwork(IHopfieldNetwork&&) = default;
    IHopfieldNetwork& operator=(IHopfieldNetwork&&) = default;

    // --- Topology ---

    /// Hypercube dimension. NumVertices() == 2^Dim().
    [[nodiscard]] virtual size_t Dim() const = 0;
    /// Number of vertices (neurons). Equal to 2^Dim().
    [[nodiscard]] virtual size_t NumVertices() const = 0;

    // --- Core operations ---

    /// Store a pattern (explicit storage, not Hebbian).
    /// @param pattern Exactly NumVertices() continuous-valued floats.
    /// @throws std::invalid_argument if pattern.size() != NumVertices().
    virtual void StorePattern(std::span<const float> pattern) = 0;

    /// Run recall sweeps until convergence or max_steps.
    /// @param state In/out: NumVertices() floats, modified in place.
    /// @param max_steps Maximum update sweeps.
    /// @param mode Async (random-order, guaranteed convergence) or
    ///             Sync (double-buffered, deterministic, GPU-portable).
    /// @throws std::invalid_argument if state.size() != NumVertices().
    virtual RecallResult Recall(std::span<float> state, size_t max_steps = 100,
                                UpdateMode mode = UpdateMode::Sync) = 0;

    /// Compute the modern Hopfield energy for the given state.
    /// @return Energy value, or nullopt if no patterns are stored.
    /// @throws std::invalid_argument if state.size() != NumVertices().
    [[nodiscard]] virtual std::optional<float> Energy(std::span<const float> state) const = 0;

    // --- Introspection (all needed for serialization/reconstruction) ---

    /// Number of stored patterns.
    [[nodiscard]] virtual size_t NumPatterns() const = 0;
    /// Original RNG seed passed at construction.
    [[nodiscard]] virtual uint64_t Seed() const = 0;
    /// Hamming-ball radius for neighbor connectivity (1 to Dim()).
    [[nodiscard]] virtual size_t Reach() const = 0;
    /// Fraction of the Hamming ball used (0.0, 1.0].
    [[nodiscard]] virtual float NeighborFraction() const = 0;
    /// Inverse temperature for softmax attention.
    [[nodiscard]] virtual float Beta() const = 0;
    /// Convergence threshold for Recall().
    [[nodiscard]] virtual float Tolerance() const = 0;

    // --- Pattern management ---

    /// Remove all stored patterns and reset state.
    virtual void Clear() = 0;
    /// Read back a stored pattern by index.
    /// @throws std::out_of_range if idx >= NumPatterns().
    [[nodiscard]] virtual std::span<const float> GetPattern(size_t idx) const = 0;
    /// Remove the most recently stored pattern.
    /// @throws std::out_of_range if NumPatterns() == 0.
    virtual void PopPattern() = 0;
};

/// Factory: create a HopfieldNetwork with DIM chosen at runtime.
/// @param dim Hypercube dimension (4-16).
/// @param rng_seed Random seed for update order permutations.
/// @param reach Max Hamming distance (0 = dim/2 default).
/// @param beta Inverse temperature for softmax attention.
/// @param neighbor_fraction Fraction of Hamming ball to use (0.0-1.0).
/// @param tolerance Convergence threshold for Recall().
/// @throws std::invalid_argument if dim is outside [4, 16], or if reach, beta,
///         neighbor_fraction, or tolerance are out of valid range.
std::unique_ptr<IHopfieldNetwork> CreateHopfieldNetwork(
    size_t dim, uint64_t rng_seed,
    size_t reach = 0,
    float beta = 4.0f,
    float neighbor_fraction = 1.0f,
    float tolerance = 1e-6f);

/// Modern Hopfield associative memory on a hypercube graph.
///
/// Based on the exponential-energy Modern Hopfield framework (Ramsauer et al.,
/// 2021), adapted to a DIM-dimensional hypercube with N = 2^DIM vertices.
/// Vertices are addressed by DIM-bit binary strings; each holds a continuous-
/// valued state. Patterns are stored explicitly (not collapsed into weights)
/// and retrieved via softmax attention over sparse local neighborhoods.
///
/// Architecture:
///   - Each vertex holds a continuous-valued state in the range produced by
///     softmax-weighted voting over stored patterns.
///   - Neighbors are all vertices within Hamming distance `reach` (the Hamming
///     ball). Neighbor lookup is a single XOR -- no adjacency storage required.
///   - Two update modes: Async (random-order sequential, guaranteed energy
///     descent) and Sync (double-buffered simultaneous, deterministic and
///     GPU-portable). Both detect convergence when no vertex changes by more
///     than `tolerance` (default 1e-6) in a full sweep.
///
/// Usage (template):
///   auto net = HopfieldNetwork<8>::Create(seed);
///   std::vector<float> pat(net->NumVertices());
///   // ... fill pat ...
///   net->StorePattern(pat);                  // stores via span overload
///   auto [steps, converged] = net->Recall(noisy_cue);  // modifies in place
///
/// Usage (factory, for SDK bindings):
///   auto net = CreateHopfieldNetwork(8, seed);  // returns unique_ptr<IHopfieldNetwork>
///   net->StorePattern(pat);
///   auto [steps, converged] = net->Recall(noisy_cue);
template <size_t DIM>
class HopfieldNetwork : public IHopfieldNetwork
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
    /// @param neighbor_fraction Fraction of the Hamming ball to use (0.0-1.0).
    ///                          Masks are sorted by distance (closest first)
    ///                          then truncated. Default 1.0 uses the full ball.
    /// @param tolerance Convergence threshold for Recall(). A sweep is
    ///                  considered stable when no vertex changes by more than
    ///                  this value.
    static std::unique_ptr<HopfieldNetwork> Create(uint64_t rng_seed,
                                                   size_t reach = DIM / 2,
                                                   float beta = 4.0f,
                                                   float neighbor_fraction = 1.0f,
                                                   float tolerance = 1e-6f)
    {
        return std::unique_ptr<HopfieldNetwork>(
            new HopfieldNetwork(rng_seed, reach, beta, neighbor_fraction, tolerance));
    }

    ~HopfieldNetwork() override;
    HopfieldNetwork(const HopfieldNetwork&) = delete;
    HopfieldNetwork& operator=(const HopfieldNetwork&) = delete;
    HopfieldNetwork(HopfieldNetwork&&) noexcept;
    HopfieldNetwork& operator=(HopfieldNetwork&&) noexcept;

    // --- IHopfieldNetwork overrides (span-based, size-validated) ---

    [[nodiscard]] size_t Dim() const override { return DIM; }
    [[nodiscard]] size_t NumVertices() const override { return N; }

    void StorePattern(std::span<const float> pattern) override
    {
        if (pattern.size() != N)
            throw std::invalid_argument("StorePattern: expected " + std::to_string(N)
                + " elements, got " + std::to_string(pattern.size()));
        StorePattern(pattern.data());
    }

    RecallResult Recall(std::span<float> state, size_t max_steps = 100,
                        UpdateMode mode = UpdateMode::Sync) override
    {
        if (state.size() != N)
            throw std::invalid_argument("Recall: expected " + std::to_string(N)
                + " elements, got " + std::to_string(state.size()));
        return Recall(state.data(), max_steps, mode);
    }

    [[nodiscard]] std::optional<float> Energy(std::span<const float> state) const override
    {
        if (state.size() != N)
            throw std::invalid_argument("Energy: expected " + std::to_string(N)
                + " elements, got " + std::to_string(state.size()));
        return Energy(state.data());
    }

    [[nodiscard]] size_t NumPatterns() const override { return num_patterns_; }
    [[nodiscard]] uint64_t Seed() const override { return seed_; }
    [[nodiscard]] size_t Reach() const override { return reach_; }
    [[nodiscard]] float NeighborFraction() const override { return neighbor_fraction_; }
    [[nodiscard]] float Beta() const override { return beta_; }
    [[nodiscard]] float Tolerance() const override { return tolerance_; }

    void Clear() override;

    [[nodiscard]] std::span<const float> GetPattern(size_t idx) const override
    {
        if (idx >= num_patterns_)
            throw std::out_of_range("GetPattern: index " + std::to_string(idx)
                + " >= num_patterns " + std::to_string(num_patterns_));
        return {patterns_.data() + idx * N, N};
    }

    void PopPattern() override
    {
        if (num_patterns_ == 0)
            throw std::out_of_range("no patterns to remove");
        patterns_.resize(patterns_.size() - N);
        --num_patterns_;
        sim_buf_.resize(num_patterns_);
        patterns_dirty_ = true;
    }

    // --- Raw-pointer convenience methods (no virtual dispatch) ---

    /// @brief Store a pattern into the network (explicit storage, not Hebbian).
    /// @param pattern Pointer to N floats (continuous-valued).
    void StorePattern(const float* pattern);

    /// @brief Run recall until convergence or max_steps.
    /// @param state In/out: pointer to N-element state array. Modified in place.
    /// @param max_steps Maximum update sweeps before declaring non-convergence.
    /// @param mode Async or Sync update strategy.
    RecallResult Recall(float* state, size_t max_steps = 100,
                        UpdateMode mode = UpdateMode::Sync);

    /// @brief Compute modern Hopfield energy for the given state.
    /// @return Energy value, or nullopt if no patterns are stored.
    [[nodiscard]] std::optional<float> Energy(const float* state) const;

private:
    HopfieldNetwork(uint64_t rng_seed, size_t reach, float beta, float neighbor_fraction, float tolerance);

    uint64_t seed_; // original RNG seed (for serialization)
    size_t reach_; // Hamming-ball radius (1..DIM)
    float beta_; // inverse temperature for softmax attention
    float neighbor_fraction_; // fraction of Hamming ball used (0.0-1.0)
    float tolerance_; // convergence threshold for Recall
    size_t num_patterns_ = 0;
    std::mt19937_64 rng_; // persists across Recall() calls for varied orderings

    std::vector<float> patterns_; // row-major [num_patterns_ * N] for StorePattern
    mutable std::vector<float> patterns_t_; // col-major [N * num_patterns_] for fast Recall
    mutable bool patterns_dirty_ = true; // true when patterns_t_ needs rebuild
    std::vector<float> sim_buf_; // reusable similarity buffer [num_patterns_] for Recall
    mutable std::vector<float> energy_buf_; // per-pattern similarity buffer for Energy() [num_patterns_]
    std::vector<float> sync_buf_; // double buffer for Sync mode [N]
    std::vector<uint32_t> perm_; // reusable permutation array for Async Recall [N]
    std::vector<uint32_t> conn_masks_; // precomputed neighbor masks: popcount(m) <= reach_

    // --- Threading ---
    static constexpr size_t kParallelWorkThreshold = 500000;
    mutable std::unique_ptr<ThreadPool> pool_;
    mutable std::vector<float> thread_sim_bufs_; // [pool.NumThreads() * num_patterns_]

    ThreadPool& EnsurePool() const;
    void EnsureThreadSimBufs() const;
    bool ShouldParallelize() const;

    void Initialize();
    void BuildMaskTable();
    void EnsureTransposed() const;
    void UpdateVertex(size_t v, const float* read_state, float* write_state, float* sim);
    float VertexEnergy(size_t v, const float* state, float* sim) const;
};

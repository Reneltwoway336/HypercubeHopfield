// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 David Liptak

#include "HopfieldNetwork.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>

// Explicit template instantiations for supported DIM values (4-16)
template class HopfieldNetwork<4>;
template class HopfieldNetwork<5>;
template class HopfieldNetwork<6>;
template class HopfieldNetwork<7>;
template class HopfieldNetwork<8>;
template class HopfieldNetwork<9>;
template class HopfieldNetwork<10>;
template class HopfieldNetwork<11>;
template class HopfieldNetwork<12>;
template class HopfieldNetwork<13>;
template class HopfieldNetwork<14>;
template class HopfieldNetwork<15>;
template class HopfieldNetwork<16>;

// --- Construction and initialization ---

template <size_t DIM>
HopfieldNetwork<DIM>::HopfieldNetwork(uint64_t rng_seed, size_t reach, float beta, float neighbor_fraction, float tolerance)
    : seed_(rng_seed), reach_(reach), beta_(beta), neighbor_fraction_(neighbor_fraction), tolerance_(tolerance), rng_(rng_seed)
{
    if (reach_ < 1 || reach_ > DIM)
        throw std::invalid_argument("reach must be in [1, " + std::to_string(DIM)
            + "], got " + std::to_string(reach_));
    if (beta_ <= 0.0f)
        throw std::invalid_argument("beta must be positive, got " + std::to_string(beta_));
    if (neighbor_fraction_ <= 0.0f || neighbor_fraction_ > 1.0f)
        throw std::invalid_argument("neighbor_fraction must be in (0.0, 1.0], got "
            + std::to_string(neighbor_fraction_));
    if (tolerance_ < 0.0f)
        throw std::invalid_argument("tolerance must be non-negative, got "
            + std::to_string(tolerance_));
    BuildMaskTable();
    Initialize();
}

template <size_t DIM>
void HopfieldNetwork<DIM>::BuildMaskTable()
{
    // Collect all masks within the Hamming ball
    conn_masks_.clear();
    for (uint32_t m = 1; m < N; ++m)
    {
        if (static_cast<size_t>(std::popcount(m)) <= reach_)
            conn_masks_.push_back(m);
    }

    // Sort by Hamming distance (closest first), stable within same distance
    std::stable_sort(conn_masks_.begin(), conn_masks_.end(),
        [](uint32_t a, uint32_t b) {
            return std::popcount(a) < std::popcount(b);
        });

    // Truncate to neighbor_fraction
    if (neighbor_fraction_ < 1.0f)
    {
        const size_t keep = std::max(size_t{1},
            static_cast<size_t>(static_cast<float>(conn_masks_.size()) * neighbor_fraction_));
        conn_masks_.resize(keep);
    }
}

template <size_t DIM>
void HopfieldNetwork<DIM>::Initialize()
{
    patterns_.clear();
    patterns_t_.clear();
    patterns_dirty_ = true;
    sim_buf_.clear();
    perm_.resize(N);
    std::iota(perm_.begin(), perm_.end(), 0);
    num_patterns_ = 0;
}

template <size_t DIM>
void HopfieldNetwork<DIM>::EnsureTransposed() const
{
    if (!patterns_dirty_) return;

    // Transpose from row-major [M * N] to col-major [N * M]
    // patterns_[mu * N + v] -> patterns_t_[v * M + mu]
    const size_t M = num_patterns_;
    patterns_t_.resize(N * M);
    for (size_t mu = 0; mu < M; ++mu)
        for (size_t v = 0; v < N; ++v)
            patterns_t_[v * M + mu] = patterns_[mu * N + v];

    patterns_dirty_ = false;
}

// --- Core operations ---

template <size_t DIM>
void HopfieldNetwork<DIM>::StorePattern(const float* pattern)
{
    // Allocate first so OOM throws before mutating state.
    patterns_.insert(patterns_.end(), pattern, pattern + N);
    sim_buf_.resize(num_patterns_ + 1);
    ++num_patterns_;
    patterns_dirty_ = true;
}

template <size_t DIM>
RecallResult HopfieldNetwork<DIM>::Recall(float* state, size_t max_steps)
{
    if (num_patterns_ == 0)
    {
        // Nothing to recall against -- return input unchanged.
        return {0, false};
    }

    EnsureTransposed();

    for (size_t step = 0; step < max_steps; ++step)
    {
        std::shuffle(perm_.begin(), perm_.end(), rng_);
        bool changed = false;

        for (size_t idx = 0; idx < N; ++idx)
        {
            const size_t v = perm_[idx];
            const float old_val = state[v];
            UpdateVertex(v, state);
            if (std::fabs(state[v] - old_val) > tolerance_)
                changed = true;
        }

        if (!changed)
            return {step + 1, true};
    }

    return {max_steps, false};
}

template <size_t DIM>
std::optional<float> HopfieldNetwork<DIM>::Energy(const float* state) const
{
    // Modern Hopfield energy: per-vertex log-sum-exp of pattern similarities.
    // Uses transposed pattern layout for cache-friendly access.
    if (num_patterns_ == 0) return std::nullopt;
    EnsureTransposed();

    const float inv_beta = 1.0f / beta_;
    const uint32_t* masks = conn_masks_.data();
    const size_t num_masks = conn_masks_.size();
    const size_t M = num_patterns_;
    const float* pt = patterns_t_.data();
    float energy = 0.0f;

    energy_buf_.resize(M);
    float* sim = energy_buf_.data();

    for (size_t v = 0; v < N; ++v)
    {
        // Zero sim buffer
        std::memset(sim, 0, M * sizeof(float));

        // Connection-outer, pattern-inner: each iteration is a saxpy
        // sim[mu] += state[nb] * patterns_t[nb * M + mu]  (contiguous access)
        for (size_t c = 0; c < num_masks; ++c)
        {
            const size_t nb = v ^ masks[c];
            const float nb_state = state[nb];
            const float* pt_nb = pt + nb * M;
            for (size_t mu = 0; mu < M; ++mu)
                sim[mu] += nb_state * pt_nb[mu];
        }

        // Find max for numerical stability
        float max_sim = -std::numeric_limits<float>::max();
        for (size_t mu = 0; mu < M; ++mu)
            if (sim[mu] > max_sim) max_sim = sim[mu];

        // Log-sum-exp
        float sum_exp = 0.0f;
        for (size_t mu = 0; mu < M; ++mu)
            sum_exp += std::exp(beta_ * (sim[mu] - max_sim));

        energy -= max_sim + inv_beta * std::log(sum_exp);
    }

    return energy / static_cast<float>(N);
}

template <size_t DIM>
void HopfieldNetwork<DIM>::Clear()
{
    Initialize();
}

template <size_t DIM>
void HopfieldNetwork<DIM>::UpdateVertex(size_t v, float* state)
{
    // Modern Hopfield update via softmax attention over stored patterns.
    // Uses transposed pattern layout + connection-outer loop for cache efficiency.
    //
    // Phase 1: Accumulate similarity to each pattern through Hamming-ball neighbors.
    //          Loop structure: for each neighbor, saxpy into sim buffer.
    //          sim[mu] += state[nb] * patterns_t[nb * M + mu]
    //
    // Phase 2: Softmax with inverse temperature beta.
    //
    // Phase 3: Weighted vote of patterns at vertex v (continuous output).

    const uint32_t* masks = conn_masks_.data();
    const size_t num_masks = conn_masks_.size();
    const size_t M = num_patterns_;
    const float* pt = patterns_t_.data();
    float* sim = sim_buf_.data();

    // Phase 1: similarity accumulation (connection-outer, pattern-inner)
    std::memset(sim, 0, M * sizeof(float));
    for (size_t c = 0; c < num_masks; ++c)
    {
        const size_t nb = v ^ masks[c];
        const float nb_state = state[nb];
        const float* pt_nb = pt + nb * M;
        for (size_t mu = 0; mu < M; ++mu)
            sim[mu] += nb_state * pt_nb[mu];
    }

    // Phase 2: softmax
    float max_sim = -std::numeric_limits<float>::max();
    for (size_t mu = 0; mu < M; ++mu)
        if (sim[mu] > max_sim) max_sim = sim[mu];

    float sum_exp = 0.0f;
    for (size_t mu = 0; mu < M; ++mu)
    {
        sim[mu] = std::exp(beta_ * (sim[mu] - max_sim));
        sum_exp += sim[mu];
    }

    // Phase 3: weighted vote at vertex v using transposed layout
    const float inv_sum = 1.0f / sum_exp;
    const float* pt_v = pt + v * M;
    float h = 0.0f;
    for (size_t mu = 0; mu < M; ++mu)
        h += (sim[mu] * inv_sum) * pt_v[mu];

    state[v] = h;
}

// --- Runtime DIM factory ---

template <size_t DIM>
static std::unique_ptr<IHopfieldNetwork> CreateForDim(
    uint64_t rng_seed, size_t reach, float beta, float neighbor_fraction, float tolerance)
{
    if (reach == 0) reach = DIM / 2;
    return HopfieldNetwork<DIM>::Create(rng_seed, reach, beta, neighbor_fraction, tolerance);
}

std::unique_ptr<IHopfieldNetwork> CreateHopfieldNetwork(
    size_t dim, uint64_t rng_seed,
    size_t reach, float beta, float neighbor_fraction, float tolerance)
{
    switch (dim)
    {
        case  4: return CreateForDim< 4>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case  5: return CreateForDim< 5>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case  6: return CreateForDim< 6>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case  7: return CreateForDim< 7>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case  8: return CreateForDim< 8>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case  9: return CreateForDim< 9>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case 10: return CreateForDim<10>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case 11: return CreateForDim<11>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case 12: return CreateForDim<12>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case 13: return CreateForDim<13>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case 14: return CreateForDim<14>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case 15: return CreateForDim<15>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        case 16: return CreateForDim<16>(rng_seed, reach, beta, neighbor_fraction, tolerance);
        default:
            throw std::invalid_argument("dim must be in [4, 16], got " + std::to_string(dim));
    }
}

#include "HopfieldNetwork.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <limits>
#include <numeric>

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
HopfieldNetwork<DIM>::HopfieldNetwork(uint64_t rng_seed, size_t reach, float beta, float connectivity)
    : reach_(reach), beta_(beta), connectivity_(connectivity), rng_(rng_seed)
{
    assert(reach_ >= 1 && reach_ <= DIM);
    assert(beta_ > 0.0f);
    assert(connectivity_ > 0.0f && connectivity_ <= 1.0f);
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

    // Truncate to connectivity fraction
    if (connectivity_ < 1.0f)
    {
        const size_t keep = std::max(size_t{1},
            static_cast<size_t>(static_cast<float>(conn_masks_.size()) * connectivity_));
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
    std::fill(vtx_state_, vtx_state_ + N, 1.0f);
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
#ifndef NDEBUG
    for (size_t i = 0; i < N; ++i)
        assert(pattern[i] == 1.0f || pattern[i] == -1.0f);
#endif
    patterns_.insert(patterns_.end(), pattern, pattern + N);
    ++num_patterns_;
    sim_buf_.resize(num_patterns_);
    patterns_dirty_ = true;
}

template <size_t DIM>
size_t HopfieldNetwork<DIM>::Recall(float* state, size_t max_steps)
{
    EnsureTransposed();

    // Copy input state into internal buffer
    std::copy(state, state + N, vtx_state_);

    // Build a permutation array for random async updates
    std::vector<size_t> perm(N);
    std::iota(perm.begin(), perm.end(), 0);

    for (size_t step = 0; step < max_steps; ++step)
    {
        std::shuffle(perm.begin(), perm.end(), rng_);
        bool changed = false;

        for (size_t idx = 0; idx < N; ++idx)
        {
            const size_t v = perm[idx];
            const float old_spin = vtx_state_[v];
            UpdateVertex(v);
            if (vtx_state_[v] != old_spin)
                changed = true;
        }

        if (!changed)
        {
            // Converged — copy result back
            std::copy(vtx_state_, vtx_state_ + N, state);
            return step + 1;
        }
    }

    // Did not converge within max_steps
    std::copy(vtx_state_, vtx_state_ + N, state);
    return max_steps;
}

template <size_t DIM>
float HopfieldNetwork<DIM>::Energy(const float* state) const
{
    // Modern Hopfield energy: per-vertex log-sum-exp of pattern similarities.
    // Uses transposed pattern layout for cache-friendly access.
    if (num_patterns_ == 0) return 0.0f;
    EnsureTransposed();

    const float inv_beta = 1.0f / beta_;
    const uint32_t* masks = conn_masks_.data();
    const size_t num_masks = conn_masks_.size();
    const size_t M = num_patterns_;
    const float* pt = patterns_t_.data();
    float energy = 0.0f;

    #pragma omp parallel reduction(+:energy)
    {
        std::vector<float> sim(M);

        #pragma omp for schedule(static)
        for (size_t v = 0; v < N; ++v)
        {
            // Zero sim buffer
            std::memset(sim.data(), 0, M * sizeof(float));

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
    }

    return energy / static_cast<float>(N);
}

template <size_t DIM>
void HopfieldNetwork<DIM>::Clear()
{
    Initialize();
}

template <size_t DIM>
void HopfieldNetwork<DIM>::UpdateVertex(size_t v)
{
    if (num_patterns_ == 0) return;

    // Modern Hopfield update via softmax attention over stored patterns.
    // Uses transposed pattern layout + connection-outer loop for cache efficiency.
    //
    // Phase 1: Accumulate similarity to each pattern through Hamming-ball neighbors.
    //          Loop structure: for each neighbor, saxpy into sim buffer.
    //          sim[mu] += state[nb] * patterns_t[nb * M + mu]
    //
    // Phase 2: Softmax with inverse temperature beta.
    //
    // Phase 3: Weighted vote of patterns at vertex v, sign activation.

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
        const float nb_state = vtx_state_[nb];
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
    const float* pt_v = pt + v * M;
    float h = 0.0f;
    for (size_t mu = 0; mu < M; ++mu)
        h += (sim[mu] / sum_exp) * pt_v[mu];

    // Sign activation
    vtx_state_[v] = (h >= 0.0f) ? 1.0f : -1.0f;
}

#include "HopfieldNetwork.h"

#include <bit>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <limits>
#include <numeric>

// Explicit template instantiations for supported DIM values
template class HopfieldNetwork<5>;
template class HopfieldNetwork<6>;
template class HopfieldNetwork<7>;
template class HopfieldNetwork<8>;
template class HopfieldNetwork<9>;
template class HopfieldNetwork<10>;

// --- Construction and initialization ---

template <size_t DIM>
HopfieldNetwork<DIM>::HopfieldNetwork(uint64_t rng_seed, size_t reach, float beta)
    : reach_(reach), beta_(beta), rng_(rng_seed)
{
    assert(reach_ >= 1 && reach_ <= DIM);
    assert(beta_ > 0.0f);
    BuildMaskTable();
    Initialize();
}

template <size_t DIM>
void HopfieldNetwork<DIM>::BuildMaskTable()
{
    conn_masks_.clear();
    for (uint32_t m = 1; m < N; ++m)
    {
        if (static_cast<size_t>(std::popcount(m)) <= reach_)
            conn_masks_.push_back(m);
    }
}

template <size_t DIM>
void HopfieldNetwork<DIM>::Initialize()
{
    patterns_.clear();
    sim_buf_.clear();
    std::fill(vtx_state_, vtx_state_ + N, 1.0f);
    num_patterns_ = 0;
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
}

template <size_t DIM>
size_t HopfieldNetwork<DIM>::Recall(float* state, size_t max_steps)
{
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
    // E(s) = -(1/N) * sum_v [ beta^-1 * log(sum_mu exp(beta * sim_mu(v))) ]
    // where sim_mu(v) sums over Hamming-ball neighbors of v.
    if (num_patterns_ == 0) return 0.0f;

    const float inv_beta = 1.0f / beta_;
    const uint32_t* masks = conn_masks_.data();
    const size_t num_masks = conn_masks_.size();
    float energy = 0.0f;

    #pragma omp parallel reduction(+:energy)
    {
        std::vector<float> sim(num_patterns_);

        #pragma omp for schedule(static)
        for (size_t v = 0; v < N; ++v)
        {
            float max_sim = -std::numeric_limits<float>::max();

            for (size_t mu = 0; mu < num_patterns_; ++mu)
            {
                const float* p = patterns_.data() + mu * N;
                float s = 0.0f;

                for (size_t c = 0; c < num_masks; ++c)
                {
                    const size_t nb = v ^ masks[c];
                    s += p[nb] * state[nb];
                }

                sim[mu] = s;
                if (s > max_sim) max_sim = s;
            }

            float sum_exp = 0.0f;
            for (size_t mu = 0; mu < num_patterns_; ++mu)
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
    //
    // 1. Compute similarity to each pattern through Hamming-ball neighbors:
    //      sim_mu = sum over all nb where popcount(v ^ nb) <= reach
    //
    // 2. Apply softmax with inverse temperature beta:
    //      alpha_mu = exp(beta * sim_mu) / sum_mu exp(beta * sim_mu)
    //
    // 3. New state is softmax-weighted vote of patterns at vertex v:
    //      h_v = sum_mu alpha_mu * pattern[mu][v]
    //      s_v = sign(h_v)

    const uint32_t* masks = conn_masks_.data();
    const size_t num_masks = conn_masks_.size();
    float max_sim = -std::numeric_limits<float>::max();
    float* sim = sim_buf_.data();

    for (size_t mu = 0; mu < num_patterns_; ++mu)
    {
        const float* p = patterns_.data() + mu * N;
        float s = 0.0f;

        for (size_t c = 0; c < num_masks; ++c)
        {
            const size_t nb = v ^ masks[c];
            s += p[nb] * vtx_state_[nb];
        }

        sim[mu] = s;
        if (s > max_sim) max_sim = s;
    }

    // --- Softmax: exp(beta * (sim - max)) / sum ---
    float sum_exp = 0.0f;
    for (size_t mu = 0; mu < num_patterns_; ++mu)
    {
        sim[mu] = std::exp(beta_ * (sim[mu] - max_sim));
        sum_exp += sim[mu];
    }

    // --- Weighted vote of patterns at vertex v ---
    float h = 0.0f;
    for (size_t mu = 0; mu < num_patterns_; ++mu)
    {
        const float* p = patterns_.data() + mu * N;
        h += (sim[mu] / sum_exp) * p[v];
    }

    // Sign activation
    vtx_state_[v] = (h >= 0.0f) ? 1.0f : -1.0f;
}

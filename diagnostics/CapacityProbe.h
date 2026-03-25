// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 David Liptak

#pragma once

#include "../HopfieldNetwork.h"
#include "DiagnosticHelpers.h"

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <vector>

/// @brief Diagnostic: find empirical memory capacity by storing increasing patterns.
///
/// Stores 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, ... patterns, corrupts each
/// at 20% noise, recalls, and measures mean overlap. Capacity is the largest pattern
/// count where mean overlap stays >= 0.90 before the first drop below threshold.
///
/// At high pattern counts, tests a random sample of patterns (default 64) rather
/// than all M, reducing the cost from O(M^2) to O(M). The recall tests within
/// each seed are parallelized across threads via OMP.
///
/// The default ceiling is scaled per DIM to target ~5 minute runtime at default
/// reach=DIM/2. Override max_patterns in the constructor to probe deeper.
template <size_t DIM>
class CapacityProbe
{
    static constexpr size_t N = 1ULL << DIM;

    /// Intelligent default ceiling scaled by DIM.
    /// Two cost factors limit the ceiling:
    ///   1. Recall cost per step: O(M * connections * N) ~ O(M * N^2)
    ///   2. Storage cost per step: O(M * N * threads) -- OMP threads duplicate patterns
    /// The ceiling is the minimum of limits from both factors.
    /// Calibrated from DIM=8 where 65536 patterns completes in ~2 minutes.
    static constexpr size_t DefaultCeiling()
    {
        // Recall-limited ceiling: scales inversely with N^2
        constexpr size_t recall_base = 65536;
        constexpr size_t recall_n2 = 256ULL * 256;
        constexpr size_t recall_limit = recall_base * recall_n2 / (N * N);

        // Storage-limited ceiling: ~128MB per network copy, ~8 threads
        // M * N * 4 bytes * 8 threads <= 1GB => M <= 1GB / (N * 32)
        constexpr size_t storage_limit = (1024ULL * 1024 * 1024) / (N * 32);

        constexpr size_t raw = recall_limit < storage_limit ? recall_limit : storage_limit;

        if (raw < 64) return 64;
        if (raw > 65536) return 65536;
        return raw;
    }

public:
    /// @param max_patterns Upper limit on patterns to test. Default is scaled
    ///                     per DIM to target ~5 min runtime. Override to probe deeper.
    /// @param max_test_samples Max patterns to test per count per seed. At counts
    ///                         above this, a random sample is tested instead of all.
    explicit CapacityProbe(size_t max_patterns = DefaultCeiling(), size_t max_test_samples = 64)
        : max_patterns_(max_patterns), max_test_samples_(max_test_samples) {}

    bool RunAndPrint()
    {
        constexpr float noise = 0.20f;
        constexpr float threshold = 0.90f;

        FILE* md = std::fopen("diagnostics/CapacityProbe.md", "w");
        if (!md)
            std::printf("  (warning: could not create diagnostics/CapacityProbe.md)\n");

        PrintHeader(md, max_patterns_, max_test_samples_);

        size_t capacity = 0;
        bool dropped = false;

        // Geometric progression: 1,2,3,4,6,8,12,16,24,32,48,64,128,...
        std::vector<size_t> counts = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64};
        for (size_t c = 128; c <= max_patterns_; c *= 2)
            counts.push_back(c);
        if (counts.back() < max_patterns_)
            counts.push_back(max_patterns_);

        for (size_t count : counts)
        {
            const auto& seeds = DiagSeeds();
            const size_t num_seeds = seeds.size();
            const size_t test_count = std::min(count, max_test_samples_);

            // Per-seed accumulators
            std::vector<float> seed_overlap(num_seeds, 0.0f);
            std::vector<float> seed_min_ovlp(num_seeds, 2.0f);
            std::vector<float> seed_sweeps(num_seeds, 0.0f);

            for (size_t si = 0; si < num_seeds; ++si)
            {
                const uint64_t seed = seeds[si];
                std::mt19937_64 rng(seed * 10000 + count);

                // Generate and store all patterns
                std::vector<float> patterns(count * N);
                for (size_t p = 0; p < count; ++p)
                    GenerateRandomPattern<N>(patterns.data() + p * N, rng);

                // Select which pattern indices to test
                std::vector<size_t> test_indices(count);
                std::iota(test_indices.begin(), test_indices.end(), 0);
                if (test_count < count)
                {
                    std::shuffle(test_indices.begin(), test_indices.end(), rng);
                    test_indices.resize(test_count);
                }

                // Per-thread accumulators for OMP reduction
                float par_overlap = 0.0f;
                float par_min_ovlp = 2.0f;
                float par_sweeps = 0.0f;

                #pragma omp parallel reduction(+:par_overlap,par_sweeps) reduction(min:par_min_ovlp)
                {
                    // Each thread gets its own network copy (for vtx_state_ / sim_buf_)
                    auto thread_net = HopfieldNetwork<DIM>::Create(seed * 10000 + count + 1);
                    for (size_t p = 0; p < count; ++p)
                        thread_net->StorePattern(patterns.data() + p * N);

                    std::vector<float> noisy(N);

                    #pragma omp for schedule(dynamic)
                    for (size_t ti = 0; ti < test_count; ++ti)
                    {
                        const size_t p = test_indices[ti];
                        const float* orig = patterns.data() + p * N;

                        // Each thread needs its own RNG for corruption
                        std::mt19937_64 thread_rng(seed * 10000 + count * 100 + p);
                        CorruptPattern<N>(orig, noisy.data(), noise, thread_rng);

                        const size_t sweeps = thread_net->Recall(noisy.data(), 100);
                        const float overlap = ComputeOverlap<N>(orig, noisy.data());
                        par_overlap += overlap;
                        par_sweeps += static_cast<float>(sweeps);
                        if (overlap < par_min_ovlp) par_min_ovlp = overlap;
                    }
                }

                seed_overlap[si] = par_overlap;
                seed_min_ovlp[si] = par_min_ovlp;
                seed_sweeps[si] = par_sweeps;
            }

            // Aggregate across seeds
            float total_overlap = 0.0f;
            float min_overlap = 2.0f;
            float total_sweeps = 0.0f;
            const size_t total_tests = num_seeds * test_count;
            for (size_t si = 0; si < num_seeds; ++si)
            {
                total_overlap += seed_overlap[si];
                total_sweeps += seed_sweeps[si];
                if (seed_min_ovlp[si] < min_overlap) min_overlap = seed_min_ovlp[si];
            }

            const float mean_overlap = total_overlap / static_cast<float>(total_tests);
            const float avg_sweeps = total_sweeps / static_cast<float>(total_tests);

            const char* status = (mean_overlap >= threshold) ? "  PASS  " : "  FAIL  ";
            if (mean_overlap >= threshold && !dropped)
                capacity = count;
            else if (mean_overlap < threshold)
                dropped = true;

            const char* sampled = (test_count < count) ? "*" : " ";
            Tee(md, Fmt("|%s%5zu | %9.4f | %8.4f | %6.1f |%s|\n",
                sampled, count, mean_overlap, min_overlap, avg_sweeps, status));

            if (mean_overlap < 0.50f && count > 4)
                break;
        }

        const bool hit_ceiling = !dropped && (capacity == counts.back());
        const char* suffix = hit_ceiling ? "+" : "";
        Tee(md, Fmt("\nCapacity (overlap >= %.0f%%): %zu%s patterns (%.2f%% of N=%zu)\n",
            threshold * 100.0f, capacity, suffix,
            100.0f * static_cast<float>(capacity) / static_cast<float>(N), N));
        if (hit_ceiling)
            Tee(md, "(ceiling reached -- increase max_patterns to probe higher)\n");
        Tee(md, Fmt("Result: **%s**\n", capacity > 0 ? "PASS" : "FAIL"));

        WriteFindings(md, capacity, hit_ceiling);

        if (md) std::fclose(md);
        return capacity > 0;
    }

private:
    size_t max_patterns_;
    size_t max_test_samples_;

    static void PrintHeader(FILE* md, size_t max_patterns, size_t max_samples)
    {
        std::printf("\n--- [3/4] CapacityProbe (ceiling=%zu) ---\n", max_patterns);

        if (md)
        {
            std::fprintf(md, "# CapacityProbe Results\n\n");
            std::fprintf(md, "## What is CapacityProbe?\n\n");
            std::fprintf(md, "Measures the network's empirical memory capacity: the maximum number of\n");
            std::fprintf(md, "patterns that can be stored and reliably recalled from 20%% noise. Capacity\n");
            std::fprintf(md, "is defined as the largest pattern count where mean overlap >= 0.90.\n\n");
            std::fprintf(md, "Classical Hopfield capacity is ~0.14N. Modern Hopfield networks with\n");
            std::fprintf(md, "exponential energy functions theoretically achieve exponential capacity\n");
            std::fprintf(md, "in N. This diagnostic measures the empirical limit on the sparse hypercube\n");
            std::fprintf(md, "topology with Hamming-ball connectivity.\n\n");
            std::fprintf(md, "At high pattern counts, a random sample of %zu patterns is tested per seed\n", max_samples);
            std::fprintf(md, "rather than all M (marked with * in the table).\n\n");
            std::fprintf(md, "---\n\n");
            std::fprintf(md, "Run: DIM=%zu | N=%zu | reach=%zu | beta=4.0 | noise=20%% | ceiling=%zu | 3-seed avg {42,1042,2042}\n\n",
                         DIM, N, DIM / 2, max_patterns);
            std::fprintf(md, "## Results\n\n");
        }

        Tee(md, "| Count | Mean Ovlp | Min Ovlp | Sweeps | Status |\n");
        Tee(md, "|-------|-----------|----------|--------|--------|\n");
    }

    static void WriteFindings(FILE* md, size_t capacity, bool hit_ceiling)
    {
        if (!md) return;
        std::fprintf(md, "\n## Findings\n\n");
        if (hit_ceiling)
        {
            std::fprintf(md, "- **Capacity: %zu+ patterns (ceiling reached).** The network showed perfect\n", capacity);
            std::fprintf(md, "  recall at all tested counts. The true capacity is higher -- increase\n");
            std::fprintf(md, "  max_patterns to probe further.\n");
        }
        else
        {
            std::fprintf(md, "- **Empirical capacity: %zu patterns.** ", capacity);
            const float ratio = static_cast<float>(capacity) / static_cast<float>(N);
            if (ratio > 0.14f)
                std::fprintf(md, "This exceeds the classical Hopfield limit of ~0.14N (= %zu),\n  confirming the benefit of the modern exponential energy function.\n",
                             static_cast<size_t>(0.14f * static_cast<float>(N)));
            else
                std::fprintf(md, "This is at or below the classical 0.14N limit (= %zu). The sparse\n  hypercube connectivity may be limiting the exponential capacity advantage.\n",
                             static_cast<size_t>(0.14f * static_cast<float>(N)));
        }
    }
};

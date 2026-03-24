#pragma once

#include "../HopfieldNetwork.h"
#include "DiagnosticHelpers.h"

#include <cstdio>
#include <cstddef>
#include <vector>

/// @brief Diagnostic: find empirical memory capacity by storing increasing patterns.
///
/// Stores 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, ... patterns, corrupts each
/// at 20% noise, recalls, and measures mean overlap. Capacity is the largest pattern
/// count where mean overlap stays >= 0.90 before the first drop below threshold.
/// This characterizes the network's fundamental storage limit at the current
/// (DIM, reach, beta) configuration.
///
/// Modern Hopfield networks theoretically achieve exponential capacity in N.
/// This diagnostic measures the empirical capacity on the sparse hypercube topology.
template <size_t DIM>
class CapacityProbe
{
    static constexpr size_t N = 1ULL << DIM;

public:
    CapacityProbe() = default;

    bool RunAndPrint()
    {
        constexpr float noise = 0.20f;
        constexpr float threshold = 0.90f;

        FILE* md = std::fopen("diagnostics/CapacityProbe.md", "w");
        if (!md)
            std::printf("  (warning: could not create diagnostics/CapacityProbe.md)\n");

        PrintHeader(md);

        size_t capacity = 0;
        bool dropped = false;

        // Geometric progression: 1,2,3,4,6,8,12,16,24,32,48,64,128,...
        std::vector<size_t> counts = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64};
        for (size_t c = 128; c <= 4 * N; c *= 2)
            counts.push_back(c);

        for (size_t count : counts)
        {
            float total_overlap = 0.0f;
            float min_overlap = 2.0f;
            float total_sweeps = 0.0f;
            size_t total_tests = 0;

            for (uint64_t seed : DiagSeeds())
            {
                std::mt19937_64 rng(seed * 10000 + count);
                auto net = HopfieldNetwork<DIM>::Create(rng(), 3, 4.0f);

                std::vector<float> patterns(count * N);
                for (size_t p = 0; p < count; ++p)
                    GenerateRandomPattern<N>(patterns.data() + p * N, rng);

                for (size_t p = 0; p < count; ++p)
                    net->StorePattern(patterns.data() + p * N);

                float noisy[N];
                for (size_t p = 0; p < count; ++p)
                {
                    const float* orig = patterns.data() + p * N;
                    (void)CorruptPattern<N>(orig, noisy, noise, rng);

                    const size_t sweeps = net->Recall(noisy, 100);
                    const float overlap = ComputeOverlap<N>(orig, noisy);
                    total_overlap += overlap;
                    total_sweeps += static_cast<float>(sweeps);
                    if (overlap < min_overlap) min_overlap = overlap;
                    ++total_tests;
                }
            }

            const float mean_overlap = total_overlap / static_cast<float>(total_tests);
            const float avg_sweeps = total_sweeps / static_cast<float>(total_tests);

            const char* status = (mean_overlap >= threshold) ? "  PASS  " : "  FAIL  ";
            if (mean_overlap >= threshold && !dropped)
                capacity = count;
            else if (mean_overlap < threshold)
                dropped = true;

            Tee(md, Fmt("| %5d | %9.4f | %8.4f | %6.1f |%s|\n",
                static_cast<int>(count), mean_overlap, min_overlap, avg_sweeps, status));

            if (mean_overlap < 0.50f && count > 4)
                break;
        }

        Tee(md, Fmt("\nCapacity (overlap >= %.0f%%): %d patterns (%.2f%% of N=%d)\n",
            threshold * 100.0f, static_cast<int>(capacity),
            100.0f * static_cast<float>(capacity) / static_cast<float>(N),
            static_cast<int>(N)));
        Tee(md, Fmt("Result: **%s**\n", capacity > 0 ? "PASS" : "FAIL"));

        WriteFindings(md, capacity);

        if (md) std::fclose(md);
        return capacity > 0;
    }

private:
    static void PrintHeader(FILE* md)
    {
        std::printf("\n--- [3/5] CapacityProbe ---\n");

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
            std::fprintf(md, "topology with dual-mask connectivity (DIM nearest + reach shells).\n\n");
            std::fprintf(md, "---\n\n");
            std::fprintf(md, "Run: DIM=%zu | N=%zu | reach=3 | beta=4.0 | noise=20%% | 3-seed avg {42,1042,2042}\n\n",
                         DIM, N);
            std::fprintf(md, "## Results\n\n");
        }

        Tee(md, "| Count | Mean Ovlp | Min Ovlp | Sweeps | Status |\n");
        Tee(md, "|-------|-----------|----------|--------|--------|\n");
    }

    static void WriteFindings(FILE* md, size_t capacity)
    {
        if (!md) return;
        std::fprintf(md, "\n## Findings\n\n");
        std::fprintf(md, "- **Empirical capacity: %zu patterns.** ", capacity);
        const float ratio = static_cast<float>(capacity) / static_cast<float>(N);
        if (ratio > 0.14f)
            std::fprintf(md, "This exceeds the classical Hopfield limit of ~0.14N (= %zu),\n  confirming the benefit of the modern exponential energy function.\n",
                         static_cast<size_t>(0.14f * static_cast<float>(N)));
        else
            std::fprintf(md, "This is at or below the classical 0.14N limit (= %zu). The sparse\n  hypercube connectivity may be limiting the exponential capacity advantage.\n",
                         static_cast<size_t>(0.14f * static_cast<float>(N)));
    }
};

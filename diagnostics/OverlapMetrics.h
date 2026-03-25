#pragma once

#include "../HopfieldNetwork.h"
#include "DiagnosticHelpers.h"

#include <cstdio>

/// @brief Diagnostic: per-pattern overlap quality and cross-pattern interference.
///
/// Stores 5 patterns, corrupts each at 20% noise, recalls, and measures:
/// - Target overlap: how well the recalled state matches the intended pattern
/// - Max cross overlap: highest similarity to any *other* stored pattern
///
/// Averaged over 3 seeds. High target overlap with low cross overlap means clean,
/// well-separated attractors. High cross overlap means spurious mixtures.
///
/// Pass: all target overlaps >= 0.90, all cross overlaps < 0.50.
template <size_t DIM>
class OverlapMetrics
{
    static constexpr size_t N = 1ULL << DIM;

public:
    OverlapMetrics() = default;

    bool RunAndPrint()
    {
        constexpr size_t num_patterns = 5;
        constexpr float noise = 0.20f;

        FILE* md = std::fopen("diagnostics/OverlapMetrics.md", "w");
        if (!md)
            std::printf("  (warning: could not create diagnostics/OverlapMetrics.md)\n");

        PrintHeader(md);

        bool pass = true;
        const auto& seeds = DiagSeeds();
        const float seed_count = static_cast<float>(seeds.size());

        // Per-pattern accumulators across seeds
        float total_target[num_patterns] = {};
        float total_cross[num_patterns] = {};
        float total_sweeps[num_patterns] = {};

        for (uint64_t seed : seeds)
        {
            std::mt19937_64 rng(seed + 4000);
            auto net = HopfieldNetwork<DIM>::Create(rng());

            std::vector<float> patterns(num_patterns * N);
            for (size_t p = 0; p < num_patterns; ++p)
            {
                GenerateRandomPattern<N>(patterns.data() + p * N, rng);
                net->StorePattern(patterns.data() + p * N);
            }

            std::vector<float> noisy(N);
            for (size_t p = 0; p < num_patterns; ++p)
            {
                const float* orig = patterns.data() + p * N;
                CorruptPattern<N>(orig, noisy.data(), noise, rng);

                const size_t sweeps = net->Recall(noisy.data(), 100);
                total_target[p] += ComputeOverlap<N>(orig, noisy.data());
                total_sweeps[p] += static_cast<float>(sweeps);

                float max_cross = -2.0f;
                for (size_t q = 0; q < num_patterns; ++q)
                {
                    if (q == p) continue;
                    const float cross = ComputeOverlap<N>(patterns.data() + q * N, noisy.data());
                    if (cross > max_cross) max_cross = cross;
                }
                total_cross[p] += max_cross;
            }
        }

        for (size_t p = 0; p < num_patterns; ++p)
        {
            const float avg_target = total_target[p] / seed_count;
            const float avg_cross = total_cross[p] / seed_count;
            const float avg_sweeps = total_sweeps[p] / seed_count;

            const bool target_ok = (avg_target >= 0.90f);
            const bool cross_ok = (avg_cross < 0.50f);
            const bool row_pass = target_ok && cross_ok;
            if (!row_pass) pass = false;

            Tee(md, Fmt("| %7d | %11.4f | %+14.4f | %6.1f | %s |\n",
                static_cast<int>(p), avg_target, avg_cross,
                avg_sweeps, row_pass ? " PASS " : " FAIL "));
        }

        Tee(md, Fmt("\nResult: **%s**\n", pass ? "PASS" : "FAIL"));

        WriteFindings(md, pass);

        if (md) std::fclose(md);
        return pass;
    }

private:
    static void PrintHeader(FILE* md)
    {
        std::printf("\n--- [4/4] OverlapMetrics ---\n");

        if (md)
        {
            std::fprintf(md, "# OverlapMetrics Results\n\n");
            std::fprintf(md, "## What is OverlapMetrics?\n\n");
            std::fprintf(md, "Measures per-pattern recall quality and cross-pattern interference.\n");
            std::fprintf(md, "For each stored pattern, we corrupt it at 20%% noise, recall, and compute:\n");
            std::fprintf(md, "- **Target overlap**: similarity to the intended pattern (should be ~1.0)\n");
            std::fprintf(md, "- **Max cross overlap**: highest similarity to any other stored pattern\n");
            std::fprintf(md, "  (should be low -- high values mean spurious attractor mixtures)\n\n");
            std::fprintf(md, "This diagnoses whether attractors are clean and well-separated, or whether\n");
            std::fprintf(md, "the network converges to spurious states that blend multiple patterns.\n\n");
            std::fprintf(md, "---\n\n");
            std::fprintf(md, "Run: DIM=%zu | N=%zu | reach=%zu | beta=4.0 | 5 patterns | noise=20%% | 3-seed avg {42,1042,2042}\n\n",
                         DIM, N, DIM / 2);
            std::fprintf(md, "## Results\n\n");
        }

        Tee(md, "| Pattern | Target Ovlp | Max Cross Ovlp | Sweeps | Result |\n");
        Tee(md, "|---------|-------------|----------------|--------|--------|\n");
    }

    static void WriteFindings(FILE* md, bool pass)
    {
        if (!md) return;
        std::fprintf(md, "\n## Findings\n\n");
        if (pass)
        {
            std::fprintf(md, "- **Attractors are clean and well-separated.** All target overlaps >= 0.90\n");
            std::fprintf(md, "  with low cross-pattern interference, indicating the softmax attention\n");
            std::fprintf(md, "  mechanism successfully isolates individual patterns.\n");
        }
        else
        {
            std::fprintf(md, "- **Some patterns show poor recall or high cross-interference.** This may\n");
            std::fprintf(md, "  indicate that 5 patterns exceeds reliable capacity at this configuration,\n");
            std::fprintf(md, "  or that some patterns are too similar by chance.\n");
        }
    }
};

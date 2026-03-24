#pragma once

#include "../HopfieldNetwork.h"
#include "DiagnosticHelpers.h"

#include <cstdio>

/// @brief Diagnostic: single-pattern recall at varying noise levels.
///
/// Stores one pattern, corrupts it at 10%-50% noise, recalls, and measures
/// overlap with the original. Averaged over 3 seeds. Tests that the network
/// can reliably recover from noise — the fundamental Hopfield operation.
///
/// Pass criteria: overlap >= 0.95 at 10% noise, >= 0.90 at 20%, >= 0.80 at 30%.
template <size_t DIM>
class NoiseRecall
{
    static constexpr size_t N = 1ULL << DIM;

public:
    NoiseRecall() = default;

    bool RunAndPrint()
    {
        constexpr float noise_levels[] = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f};
        constexpr float thresholds[]   = {0.95f, 0.90f, 0.80f, 0.0f,  0.0f};
        constexpr size_t num_levels = sizeof(noise_levels) / sizeof(noise_levels[0]);

        FILE* md = std::fopen("diagnostics/NoiseRecall.md", "w");
        if (!md)
            std::printf("  (warning: could not create diagnostics/NoiseRecall.md)\n");

        PrintHeader(md);

        bool pass = true;
        const auto& seeds = DiagSeeds();
        const float seed_count = static_cast<float>(seeds.size());

        for (size_t lvl = 0; lvl < num_levels; ++lvl)
        {
            float total_overlap = 0.0f;
            float total_sweeps = 0.0f;

            for (uint64_t seed : seeds)
            {
                std::mt19937_64 rng(seed + lvl * 1000);
                auto net = HopfieldNetwork<DIM>::Create(rng(), 3, 4.0f);

                float pattern[N];
                GenerateRandomPattern<N>(pattern, rng);
                net->StorePattern(pattern);

                float noisy[N];
                (void)CorruptPattern<N>(pattern, noisy, noise_levels[lvl], rng);

                const size_t sweeps = net->Recall(noisy, 100);
                total_overlap += ComputeOverlap<N>(pattern, noisy);
                total_sweeps += static_cast<float>(sweeps);
            }

            const float avg_overlap = total_overlap / seed_count;
            const float avg_sweeps = total_sweeps / seed_count;
            const bool level_pass = (thresholds[lvl] <= 0.0f) || (avg_overlap >= thresholds[lvl]);
            if (!level_pass) pass = false;

            const char* result = (thresholds[lvl] <= 0.0f) ? "  --"
                               : (level_pass ? "PASS" : "FAIL");

            Tee(md, Fmt("| %4.0f%% | %.4f  | %5.1f  | %s |\n",
                noise_levels[lvl] * 100.0f, avg_overlap, avg_sweeps, result));
        }

        Tee(md, Fmt("\nResult: **%s**\n", pass ? "PASS" : "FAIL"));

        WriteFindings(md, pass);

        if (md) std::fclose(md);
        return pass;
    }

private:
    static void PrintHeader(FILE* md)
    {
        std::printf("\n--- [1/5] NoiseRecall ---\n");

        if (md)
        {
            std::fprintf(md, "# NoiseRecall Results\n\n");
            std::fprintf(md, "## What is NoiseRecall?\n\n");
            std::fprintf(md, "Measures the network's ability to recover a stored pattern from a noisy cue.\n");
            std::fprintf(md, "A single pattern is stored, then corrupted at varying noise levels (percentage\n");
            std::fprintf(md, "of bits flipped). The network runs Recall() and the overlap (normalized dot\n");
            std::fprintf(md, "product) between the recalled state and the original is measured. An overlap\n");
            std::fprintf(md, "of 1.0 means perfect recall; 0.0 means uncorrelated.\n\n");
            std::fprintf(md, "---\n\n");
            std::fprintf(md, "Run: DIM=%zu | N=%zu | reach=3 | beta=4.0 | 3-seed avg {42,1042,2042}\n\n",
                         DIM, N);
            std::fprintf(md, "## Results\n\n");
        }

        Tee(md, "| Noise | Overlap | Sweeps | Result |\n");
        Tee(md, "|-------|---------|--------|--------|\n");
    }

    static void WriteFindings(FILE* md, bool pass)
    {
        if (!md) return;
        std::fprintf(md, "\n## Findings\n\n");
        if (pass)
        {
            std::fprintf(md, "- **Network recovers reliably from moderate noise.** Overlap remains high\n");
            std::fprintf(md, "  through 30%% corruption, demonstrating robust attractor basins.\n");
        }
        else
        {
            std::fprintf(md, "- **Recall failed at one or more noise thresholds.** The attractor basins\n");
            std::fprintf(md, "  may be too shallow at this configuration (reach=3, beta=4.0).\n");
        }
    }
};

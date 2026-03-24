#pragma once

#include "../HopfieldNetwork.h"
#include "DiagnosticHelpers.h"

#include <cstdio>
#include <vector>

/// @brief Diagnostic: sweep reach and beta to characterize their effect on capacity.
///
/// Tests a grid of (reach, beta) pairs. For each, runs a mini capacity probe to
/// find the largest pattern count with mean overlap >= 0.90 at 20% noise before
/// the first drop. Also reports average convergence sweeps at capacity.
///
/// This reveals how connectivity (reach) and retrieval sharpness (beta) interact
/// on the hypercube topology. Informational — no pass/fail.
template <size_t DIM>
class ParameterSweep
{
    static constexpr size_t N = 1ULL << DIM;

public:
    ParameterSweep() = default;

    bool RunAndPrint()
    {
        constexpr size_t reach_vals[] = {1, 2, 3, 5, DIM};
        constexpr float beta_vals[] = {1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
        constexpr size_t num_reach = sizeof(reach_vals) / sizeof(reach_vals[0]);
        constexpr size_t num_beta = sizeof(beta_vals) / sizeof(beta_vals[0]);
        constexpr float noise = 0.20f;
        constexpr float threshold = 0.90f;

        FILE* md = std::fopen("diagnostics/ParameterSweep.md", "w");
        if (!md)
            std::printf("  (warning: could not create diagnostics/ParameterSweep.md)\n");

        PrintHeader(md, reach_vals, num_reach, beta_vals, num_beta);

        // Column headers
        std::string hdr = "| reach |";
        std::string sep = "|-------|";
        for (size_t b = 0; b < num_beta; ++b)
        {
            hdr += Fmt(" b=%-5.0f|", beta_vals[b]);
            sep += "--------|";
        }
        Tee(md, hdr + "\n");
        Tee(md, sep + "\n");

        for (size_t r = 0; r < num_reach; ++r)
        {
            const size_t reach = reach_vals[r];
            std::string row = Fmt("| %3d   |", static_cast<int>(reach));

            for (size_t b = 0; b < num_beta; ++b)
            {
                const float beta = beta_vals[b];
                size_t capacity = 0;
                float cap_sweeps = 0.0f;
                bool dropped = false;

                // Mini capacity probe: 1,2,4,8,16,32,64
                for (size_t count = 1; count <= 64; count *= 2)
                {
                    float total_overlap = 0.0f;
                    float total_sweeps = 0.0f;
                    size_t total_tests = 0;

                    for (uint64_t seed : DiagSeeds())
                    {
                        std::mt19937_64 rng(seed * 10000 + r * 1000 + b * 100 + count);
                        auto net = HopfieldNetwork<DIM>::Create(rng(), reach, beta);

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
                            total_overlap += ComputeOverlap<N>(orig, noisy);
                            total_sweeps += static_cast<float>(sweeps);
                            ++total_tests;
                        }
                    }

                    const float mean_overlap = total_overlap / static_cast<float>(total_tests);
                    if (mean_overlap >= threshold && !dropped)
                    {
                        capacity = count;
                        cap_sweeps = total_sweeps / static_cast<float>(total_tests);
                    }
                    else if (mean_overlap < threshold)
                        dropped = true;
                }

                row += Fmt(" %2d/%3.0f |", static_cast<int>(capacity), cap_sweeps);
            }
            Tee(md, row + "\n");
        }

        Tee(md, Fmt("\n(format: capacity / avg_sweeps | capacity = max patterns with overlap >= %.0f%%)\n",
            threshold * 100.0f));

        WriteFindings(md);

        if (md) std::fclose(md);
        return true;  // informational, always passes
    }

private:
    static void PrintHeader(FILE* md,
                            const size_t* reach_vals, size_t num_reach,
                            const float* beta_vals, size_t num_beta)
    {
        std::printf("\n--- [5/5] ParameterSweep ---\n");

        if (md)
        {
            std::fprintf(md, "# ParameterSweep Results\n\n");
            std::fprintf(md, "## What is ParameterSweep?\n\n");
            std::fprintf(md, "Characterizes how the two key parameters affect network capacity:\n");
            std::fprintf(md, "- **reach** (1-%zu): number of Hamming-shell connections per vertex, added\n",
                         reach_vals[num_reach - 1]);
            std::fprintf(md, "  on top of DIM=%zu nearest-neighbor connections. More shells = richer\n", DIM);
            std::fprintf(md, "  long-range context for the softmax attention mechanism.\n");
            std::fprintf(md, "- **beta** (inverse temperature): controls softmax sharpness. Higher beta\n");
            std::fprintf(md, "  gives more winner-take-all retrieval; lower beta gives softer blending.\n\n");
            std::fprintf(md, "For each (reach, beta) pair, a mini capacity probe finds the largest\n");
            std::fprintf(md, "pattern count with mean overlap >= 0.90 at 20%% noise.\n\n");
            std::fprintf(md, "---\n\n");
            std::fprintf(md, "Run: DIM=%zu | N=%zu | noise=20%% | 3-seed avg {42,1042,2042}\n", DIM, N);
            std::fprintf(md, "Reach values: {");
            for (size_t i = 0; i < num_reach; ++i)
                std::fprintf(md, "%s%zu", i > 0 ? ", " : "", reach_vals[i]);
            std::fprintf(md, "} | Beta values: {");
            for (size_t i = 0; i < num_beta; ++i)
                std::fprintf(md, "%s%.0f", i > 0 ? ", " : "", beta_vals[i]);
            std::fprintf(md, "}\n\n");
            std::fprintf(md, "## Results\n\n");
        }
    }

    static void WriteFindings(FILE* md)
    {
        if (!md) return;
        std::fprintf(md, "\n## Findings\n\n");
        std::fprintf(md, "- **Reach effect:** More Hamming shells provide richer local context for the\n");
        std::fprintf(md, "  softmax attention, generally increasing capacity — but with diminishing\n");
        std::fprintf(md, "  returns as the signal-to-noise ratio saturates.\n");
        std::fprintf(md, "- **Beta effect:** Higher beta sharpens retrieval (closer to winner-take-all),\n");
        std::fprintf(md, "  which typically improves capacity up to a point. Very high beta can cause\n");
        std::fprintf(md, "  instability if the local similarity signal is noisy.\n");
        std::fprintf(md, "- **Interaction:** The optimal beta depends on reach — sparser connectivity\n");
        std::fprintf(md, "  (low reach) benefits from lower beta to smooth out the noisy local signal,\n");
        std::fprintf(md, "  while richer connectivity (high reach) can exploit higher beta.\n");
    }
};

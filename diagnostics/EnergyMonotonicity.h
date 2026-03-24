#pragma once

#include "../HopfieldNetwork.h"
#include "DiagnosticHelpers.h"

#include <cmath>
#include <cstdio>

/// @brief Diagnostic: verify energy is non-increasing across recall sweeps.
///
/// Stores 3 patterns, starts from a random state, and runs single-sweep Recall()
/// steps while tracking energy. Classical and modern Hopfield theory guarantees
/// monotonic energy descent under asynchronous updates. Violations indicate a
/// bug in the energy function or update rule.
///
/// Tested across all 3 seeds. Pass criterion: no energy increase > epsilon (1e-3)
/// between consecutive sweeps on any seed.
template <size_t DIM>
class EnergyMonotonicity
{
    static constexpr size_t N = 1ULL << DIM;

public:
    EnergyMonotonicity() = default;

    bool RunAndPrint()
    {
        FILE* md = std::fopen("diagnostics/EnergyMonotonicity.md", "w");
        if (!md)
            std::printf("  (warning: could not create diagnostics/EnergyMonotonicity.md)\n");

        PrintHeader(md);

        constexpr float epsilon = 1e-3f;
        constexpr size_t max_sweeps = 50;
        bool pass = true;
        int total_violations = 0;

        for (uint64_t seed : DiagSeeds())
        {
            std::mt19937_64 rng(seed + 2000);
            auto net = HopfieldNetwork<DIM>::Create(rng(), 3, 4.0f);

            // Store 3 patterns
            float pattern[N];
            for (int p = 0; p < 3; ++p)
            {
                GenerateRandomPattern<N>(pattern, rng);
                net->StorePattern(pattern);
            }

            // Start from random state
            float state[N];
            GenerateRandomPattern<N>(state, rng);

            Tee(md, Fmt("\n  Seed %d:\n", static_cast<int>(seed)));
            Tee(md, "| Sweep |     Energy |    Delta | Result |\n");
            Tee(md, "|-------|------------|----------|--------|\n");

            float prev_energy = net->Energy(state);
            Tee(md, Fmt("| %5d | %10.4f |       -- |   --   |\n", 0, prev_energy));

            size_t stable_count = 0;

            for (size_t sweep = 1; sweep <= max_sweeps; ++sweep)
            {
                net->Recall(state, 1);
                const float energy = net->Energy(state);
                const float delta = energy - prev_energy;

                const bool mono = (delta <= epsilon);
                if (!mono)
                {
                    pass = false;
                    ++total_violations;
                }

                const char* result = mono ? "  PASS  " : "  FAIL  ";
                Tee(md, Fmt("| %5d | %10.4f | %+8.5f |%s|\n",
                    static_cast<int>(sweep), energy, delta, result));

                if (std::fabs(delta) < 1e-6f)
                    ++stable_count;
                else
                    stable_count = 0;

                if (stable_count >= 3)
                {
                    Tee(md, Fmt("\n(converged at sweep %d)\n", static_cast<int>(sweep)));
                    break;
                }

                prev_energy = energy;
            }
        }

        Tee(md, Fmt("\nResult: **%s**", pass ? "PASS" : "FAIL"));
        if (total_violations > 0)
            Tee(md, Fmt(" (%d violation(s) across %d seeds, epsilon=%.0e)",
                total_violations, static_cast<int>(DiagSeeds().size()), epsilon));
        Tee(md, "\n");

        WriteFindings(md, pass, total_violations);

        if (md) std::fclose(md);
        return pass;
    }

private:
    static void PrintHeader(FILE* md)
    {
        std::printf("\n--- [2/5] EnergyMonotonicity ---\n");

        if (md)
        {
            std::fprintf(md, "# EnergyMonotonicity Results\n\n");
            std::fprintf(md, "## What is EnergyMonotonicity?\n\n");
            std::fprintf(md, "Verifies that the network energy E(s) is non-increasing across asynchronous\n");
            std::fprintf(md, "update sweeps during recall. This is a fundamental guarantee of Hopfield\n");
            std::fprintf(md, "networks: each update step should either decrease energy or leave it unchanged.\n");
            std::fprintf(md, "An energy increase indicates a bug in the update rule or energy function.\n\n");
            std::fprintf(md, "The modern Hopfield energy is:\n\n");
            std::fprintf(md, "    E(s) = -(1/N) * sum_v [ beta^-1 * log(sum_mu exp(beta * sim_mu(v))) ]\n\n");
            std::fprintf(md, "---\n\n");
            std::fprintf(md, "Run: DIM=%zu | N=%zu | reach=3 | beta=4.0 | 3 stored patterns | 3-seed {42,1042,2042}\n\n",
                         DIM, N);
            std::fprintf(md, "## Results\n\n");
        }
    }

    static void WriteFindings(FILE* md, bool pass, int violations)
    {
        if (!md) return;
        std::fprintf(md, "\n## Findings\n\n");
        if (pass)
        {
            std::fprintf(md, "- **Energy is strictly non-increasing across all seeds.** All consecutive\n");
            std::fprintf(md, "  sweep pairs show delta <= 0 (within float tolerance), confirming the\n");
            std::fprintf(md, "  update rule and energy function are consistent.\n");
        }
        else
        {
            std::fprintf(md, "- **Energy monotonicity violated %d time(s).** This suggests a mismatch\n", violations);
            std::fprintf(md, "  between the update rule and energy function, or floating-point instability\n");
            std::fprintf(md, "  under -ffast-math.\n");
        }
    }
};

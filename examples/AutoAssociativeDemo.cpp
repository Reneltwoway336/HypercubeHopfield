// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 David Liptak

/// Autoassociative memory demo: sensor fault recovery.
///
/// A facility's sensor array stores known-good profiles. When sensors
/// malfunction, the network recovers correct readings from corrupted input.

#include "HopfieldNetwork.h"
#include "diagnostics/DiagnosticHelpers.h"

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

static constexpr size_t DIM = 8;
static constexpr size_t N = 1ULL << DIM;
static constexpr int num_profiles = 20;

int main()
{
    std::printf("\n");
    std::printf("============================================================\n");
    std::printf("  AUTOASSOCIATIVE MEMORY: Sensor Fault Recovery\n");
    std::printf("============================================================\n");
    std::printf("\n");
    std::printf("Imagine a facility monitored by %zu sensors -- temperature,\n", N);
    std::printf("pressure, vibration, flow rate -- spread across a plant floor.\n");
    std::printf("The network has learned %d known-good operating profiles.\n\n", num_profiles);
    std::printf("When sensors fail or return garbage, the network uses the\n");
    std::printf("remaining good readings to reconstruct what the faulty sensors\n");
    std::printf("should be reporting. This is content-addressable error\n");
    std::printf("correction: the partial input is enough to recall the whole.\n");
    std::printf("\n");
    std::printf("  DIM = %zu    sensors = %zu    stored profiles = %d\n\n", DIM, N, num_profiles);

    std::mt19937_64 rng(42);
    auto net = HopfieldNetwork<DIM>::Create(/*seed=*/123);

    std::vector<std::vector<float>> profiles(num_profiles, std::vector<float>(N));
    for (int p = 0; p < num_profiles; ++p)
    {
        GenerateRandomPattern<N>(profiles[p].data(), rng);
        net->StorePattern(profiles[p].data());
    }

    // ---- Gaussian noise corruption ----

    const int test_profile = 0;
    std::printf("------------------------------------------------------------\n");
    std::printf("  Test 1: Gaussian Noise on All Sensors\n");
    std::printf("------------------------------------------------------------\n\n");
    std::printf("Every sensor drifts from its true value by a random amount.\n");
    std::printf("Noise level is the standard deviation of the drift relative\n");
    std::printf("to signal values in [-1, 1]. Higher noise = more corruption.\n\n");

    const float noise_levels[] = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 5.0f};
    const size_t num_noise = std::size(noise_levels);

    std::printf("  %-8s  %-12s  %-12s  %-8s  %s\n",
                "Noise", "Before", "After", "Sweeps", "Result");
    std::printf("  %-8s  %-12s  %-12s  %-8s  %s\n",
                "-----", "------", "-----", "------", "------");

    float breaking_point = 0.0f;
    for (size_t t = 0; t < num_noise; ++t)
    {
        std::vector<float> corrupted(N);
        CorruptPattern<N>(profiles[test_profile].data(), corrupted.data(),
                          noise_levels[t], rng);

        const float before = ComputeOverlap<N>(
            profiles[test_profile].data(), corrupted.data());

        const auto [steps, converged] = net->Recall(corrupted.data());

        const float after = ComputeOverlap<N>(
            profiles[test_profile].data(), corrupted.data());

        const bool recovered = after > 0.99f;
        if (recovered && noise_levels[t] > breaking_point)
            breaking_point = noise_levels[t];

        std::printf("  %-8.1f  %-12.4f  %-12.4f  %-8zu  %s\n",
                    noise_levels[t], before, after, steps,
                    recovered ? "RECOVERED" : "FAILED");
    }

    std::printf("\n");
    if (breaking_point > 0.0f)
    {
        std::printf("The network recovers perfectly up to noise = %.1f.\n", breaking_point);
        std::printf("At that level, the input similarity to the true profile is already\n");
        std::printf("very low -- yet the attractor basin pulls it back completely.\n");
        std::printf("Beyond that threshold, the corrupted state lands in the wrong\n");
        std::printf("basin and converges to a different attractor.\n");
    }

    // ---- Sensor dropout ----

    std::printf("\n");
    std::printf("------------------------------------------------------------\n");
    std::printf("  Test 2: Sensor Dropout (Dead Sensors Reporting Zero)\n");
    std::printf("------------------------------------------------------------\n\n");
    std::printf("A percentage of sensors go completely dead, reporting 0.0\n");
    std::printf("instead of their true values. The network must infer the\n");
    std::printf("missing readings from the survivors alone.\n\n");

    const int dropout_pcts[] = {10, 30, 50, 70, 90};
    const size_t num_dropout = std::size(dropout_pcts);

    std::printf("  %-10s  %-10s  %-12s  %-12s  %-8s  %s\n",
                "Dead %", "Dead #", "Before", "After", "Sweeps", "Result");
    std::printf("  %-10s  %-10s  %-12s  %-12s  %-8s  %s\n",
                "------", "------", "------", "-----", "------", "------");

    int max_survived = 0;
    for (size_t d = 0; d < num_dropout; ++d)
    {
        std::vector<float> dropout(profiles[test_profile].begin(),
                                   profiles[test_profile].end());
        const size_t num_dead = N * dropout_pcts[d] / 100;
        std::vector<size_t> indices(N);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng);
        for (size_t i = 0; i < num_dead; ++i)
            dropout[indices[i]] = 0.0f;

        const float before = ComputeOverlap<N>(
            profiles[test_profile].data(), dropout.data());

        const auto [steps, converged] = net->Recall(dropout.data());

        const float after = ComputeOverlap<N>(
            profiles[test_profile].data(), dropout.data());

        const bool recovered = after > 0.99f;
        if (recovered) max_survived = dropout_pcts[d];

        std::printf("  %-10d  %-10zu  %-12.4f  %-12.4f  %-8zu  %s\n",
                    dropout_pcts[d], num_dead, before, after, steps,
                    recovered ? "RECOVERED" : "FAILED");
    }

    std::printf("\n");
    if (max_survived > 0)
    {
        std::printf("Even with %d%% of sensors dead, the network reconstructs the\n", max_survived);
        std::printf("full profile perfectly. The surviving sensors provide enough\n");
        std::printf("context to identify which stored profile matches, and the\n");
        std::printf("Hopfield dynamics fill in every missing value.\n\n");
        std::printf("This is the practical value of associative memory: partial\n");
        std::printf("information is sufficient for complete reconstruction, as long\n");
        std::printf("as the input falls within the correct attractor basin.\n");
    }

    std::printf("\n============================================================\n\n");
    return 0;
}

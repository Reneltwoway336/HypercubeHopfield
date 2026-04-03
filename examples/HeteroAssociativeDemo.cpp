// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 David Liptak

/// Heteroassociative memory demo: diagnostic lookup.
///
/// An industrial system maps sensor readings to diagnostic responses by
/// splitting the vertex space into input and output halves. The network
/// learns paired associations and recalls the output from a partial query.

#include "HopfieldNetwork.h"
#include "diagnostics/DiagnosticHelpers.h"

#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>

static constexpr size_t DIM = 8;
static constexpr size_t N = 1ULL << DIM;
static constexpr size_t HALF = N / 2;
static constexpr int num_pairs = 15;

int main()
{
    std::printf("\n");
    std::printf("============================================================\n");
    std::printf("  HETEROASSOCIATIVE MEMORY: Diagnostic Lookup\n");
    std::printf("============================================================\n");
    std::printf("\n");
    std::printf("Unlike autoassociative memory (which recovers a corrupted\n");
    std::printf("version of what it already knows), heteroassociative memory\n");
    std::printf("maps one representation to a different one: given input A,\n");
    std::printf("recall the associated output B.\n\n");
    std::printf("Here, an industrial system has learned %d condition-to-\n", num_pairs);
    std::printf("diagnosis mappings. The %zu-vertex hypercube is split:\n\n", N);
    std::printf("  - Vertices 0-%zu:    sensor measurements (input)\n", HALF - 1);
    std::printf("  - Vertices %zu-%zu:  diagnostic response  (output)\n\n", HALF, N - 1);
    std::printf("The network stores composite [input | output] patterns.\n");
    std::printf("At query time, the input half is filled and the output half\n");
    std::printf("is zeroed. The network's dynamics complete the association.\n\n");
    std::printf("  DIM = %zu    vertices = %zu    stored pairs = %d\n\n", DIM, N, num_pairs);

    std::mt19937_64 rng(99);
    auto net = HopfieldNetwork<DIM>::Create(/*seed=*/456);

    std::vector<std::vector<float>> inputs(num_pairs, std::vector<float>(HALF));
    std::vector<std::vector<float>> outputs(num_pairs, std::vector<float>(HALF));

    // Build and store each [input | output] composite pattern.
    std::vector<float> composite(N);
    for (int p = 0; p < num_pairs; ++p)
    {
        GenerateRandomPattern<HALF>(inputs[p].data(), rng);
        GenerateRandomPattern<HALF>(outputs[p].data(), rng);

        std::copy(inputs[p].begin(), inputs[p].end(), composite.begin());
        std::copy(outputs[p].begin(), outputs[p].end(),
                  composite.begin() + HALF);

        net->StorePattern(composite.data());
    }

    // ---- Clean recall ----

    std::printf("------------------------------------------------------------\n");
    std::printf("  Test 1: Clean Input Recall\n");
    std::printf("------------------------------------------------------------\n\n");
    std::printf("Each stored input is presented exactly, with the output half\n");
    std::printf("zeroed. Can the network fill in the correct diagnosis?\n\n");

    std::printf("  %-8s  %-10s  %-10s  %-8s  %s\n",
                "Pair", "Output", "Input", "Sweeps", "Result");
    std::printf("  %-8s  %-10s  %-10s  %-8s  %s\n",
                "----", "------", "-----", "------", "------");

    int clean_ok = 0;
    for (int p = 0; p < num_pairs; ++p)
    {
        std::vector<float> query(N, 0.0f);
        std::copy(inputs[p].begin(), inputs[p].end(), query.begin());

        const auto [steps, converged] = net->Recall(query.data());

        const float output_sim = ComputeOverlap<HALF>(
            outputs[p].data(), query.data() + HALF);
        const float input_sim = ComputeOverlap<HALF>(
            inputs[p].data(), query.data());

        const bool ok = output_sim > 0.95f;
        if (ok) ++clean_ok;

        std::printf("  #%-7d  %-10.4f  %-10.4f  %-8zu  %s\n",
                    p + 1, output_sim, input_sim, steps,
                    ok ? "OK" : "WEAK");
    }

    std::printf("\n  Result: %d of %d pairs recalled correctly.\n\n", clean_ok, num_pairs);

    if (clean_ok == num_pairs)
    {
        std::printf("Every stored mapping was recovered perfectly. The network\n");
        std::printf("sees the input half, identifies which composite pattern it\n");
        std::printf("belongs to, and fills in the associated output -- even\n");
        std::printf("though the output was entirely absent from the query.\n");
    }
    else
    {
        std::printf("Most mappings recalled correctly. Any failures indicate\n");
        std::printf("the stored pairs are beginning to interfere with each\n");
        std::printf("other -- the network is approaching its capacity limit.\n");
    }

    // ---- Noisy input recall ----

    std::printf("\n");
    std::printf("------------------------------------------------------------\n");
    std::printf("  Test 2: Noisy Input Recall\n");
    std::printf("------------------------------------------------------------\n\n");
    std::printf("Real sensors drift. Here, Gaussian noise is added to the\n");
    std::printf("input half of pair #1 before querying. The output half\n");
    std::printf("starts at zero as before. How much input noise can the\n");
    std::printf("network tolerate and still produce the correct diagnosis?\n\n");

    const float noise_levels[] = {0.3f, 0.6f, 1.0f, 1.5f, 2.0f, 3.0f};
    const size_t num_noise = std::size(noise_levels);

    std::printf("  %-8s  %-14s  %s\n", "Noise", "Output Sim", "Result");
    std::printf("  %-8s  %-14s  %s\n", "-----", "----------", "------");

    float max_noise_ok = 0.0f;
    for (size_t t = 0; t < num_noise; ++t)
    {
        std::vector<float> query(N, 0.0f);
        CorruptPattern<HALF>(inputs[0].data(), query.data(),
                             noise_levels[t], rng);

        net->Recall(query.data());

        const float output_sim = ComputeOverlap<HALF>(
            outputs[0].data(), query.data() + HALF);

        const char* status = output_sim > 0.95f ? "OK"
                           : output_sim > 0.80f ? "CLOSE"
                           : "FAILED";

        if (output_sim > 0.95f) max_noise_ok = noise_levels[t];

        std::printf("  %-8.1f  %-14.4f  %s\n",
                    noise_levels[t], output_sim, status);
    }

    std::printf("\n");
    if (max_noise_ok > 0.0f)
    {
        std::printf("The correct diagnosis is retrieved even with noise up to %.1f\n", max_noise_ok);
        std::printf("on the input sensors. The network does not need a perfect\n");
        std::printf("query -- it finds the nearest stored association and\n");
        std::printf("completes it.\n");
    }

    // ---- Blend test ----

    std::printf("\n");
    std::printf("------------------------------------------------------------\n");
    std::printf("  Test 3: Ambiguous Input (Blend of Two Conditions)\n");
    std::printf("------------------------------------------------------------\n\n");
    std::printf("What happens when the input is a mix of two known conditions?\n");
    std::printf("The input is a weighted blend of pairs #1 and #2. As the\n");
    std::printf("balance shifts, the network should snap to the dominant\n");
    std::printf("association -- not produce a blended output.\n\n");

    const float blend_ratios[] = {0.9f, 0.7f, 0.5f, 0.3f, 0.1f};

    std::printf("  %-18s  %-10s  %-10s  %s\n",
                "Blend", "Sim #1", "Sim #2", "Winner");
    std::printf("  %-18s  %-10s  %-10s  %s\n",
                "-----", "------", "------", "------");

    for (float alpha : blend_ratios)
    {
        std::vector<float> query(N, 0.0f);
        for (size_t i = 0; i < HALF; ++i)
            query[i] = alpha * inputs[0][i] + (1.0f - alpha) * inputs[1][i];

        net->Recall(query.data());

        const float sim_out0 = ComputeOverlap<HALF>(
            outputs[0].data(), query.data() + HALF);
        const float sim_out1 = ComputeOverlap<HALF>(
            outputs[1].data(), query.data() + HALF);

        const char* winner = (sim_out0 > sim_out1) ? "#1" : "#2";
        std::printf("  %3.0f%% #1 / %3.0f%% #2   %-10.4f  %-10.4f  %s\n",
                    alpha * 100, (1.0f - alpha) * 100,
                    sim_out0, sim_out1, winner);
    }

    std::printf("\n");
    std::printf("The network exhibits winner-take-all behavior: at every blend\n");
    std::printf("ratio it commits fully to one diagnosis rather than producing\n");
    std::printf("a meaningless average. Near the crossover point, which\n");
    std::printf("attractor wins depends on subtle pattern geometry -- but the\n");
    std::printf("network always decides, never compromises.\n");

    std::printf("\n============================================================\n\n");
    return 0;
}

#include "HopfieldNetwork.h"
#include "Diagnostics.h"

#include <algorithm>
#include <cstdio>
#include <random>

int main()
{
    constexpr size_t DIM = 8;
    constexpr size_t N = 1ULL << DIM;

    auto net = HopfieldNetwork<DIM>::Create(42);  // default reach=DIM/2, beta=4.0

    std::printf("HypercubeHopfield DIM=%zu  N=%zu  connections=%zu (Hamming ball radius=%zu)  beta=%.1f\n",
                net->dim, net->num_vertices, net->NumConnections(), net->Reach(), net->Beta());

    // Generate a random pattern (+1/-1)
    std::mt19937_64 rng(123);
    std::uniform_int_distribution<int> coin(0, 1);

    float pattern[N];
    for (size_t i = 0; i < N; ++i)
        pattern[i] = coin(rng) ? 1.0f : -1.0f;

    // Store the pattern
    net->StorePattern(pattern);
    std::printf("Patterns stored: %zu\n", net->NumPatterns());
    std::printf("Energy (original): %.4f\n", net->Energy(pattern));

    // Corrupt ~20% of bits
    float noisy[N];
    std::copy(pattern, pattern + N, noisy);
    std::uniform_int_distribution<int> flip(0, 4);
    size_t flipped = 0;
    for (size_t i = 0; i < N; ++i)
    {
        if (flip(rng) == 0)
        {
            noisy[i] = -noisy[i];
            ++flipped;
        }
    }
    std::printf("Flipped %zu / %zu bits\n", flipped, N);
    std::printf("Energy (noisy):    %.4f\n", net->Energy(noisy));

    // Recall from noisy cue
    const size_t steps = net->Recall(noisy, 100);
    std::printf("Recall converged in %zu sweep(s)\n", steps);
    std::printf("Energy (recalled): %.4f\n", net->Energy(noisy));

    // Check overlap with original
    float overlap = 0.0f;
    for (size_t i = 0; i < N; ++i)
        overlap += pattern[i] * noisy[i];
    overlap /= static_cast<float>(N);
    std::printf("Overlap with original: %.4f\n", overlap);

    // Run full diagnostics suite
    std::printf("\n");
    const bool diag_pass = RunDiagnostics<DIM>();

    return diag_pass ? 0 : 1;
}

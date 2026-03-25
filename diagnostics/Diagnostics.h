#pragma once

#include "NoiseRecall.h"
#include "EnergyMonotonicity.h"
#include "CapacityProbe.h"
#include "OverlapMetrics.h"

#include <cstdio>

/// Run all diagnostics for a given DIM. Prints results to stdout and writes
/// individual .md result files to diagnostics/.
/// Returns true if all pass/fail checks passed.
template <size_t DIM>
bool RunDiagnostics()
{
    constexpr size_t N = 1ULL << DIM;

    std::printf("============================================================\n");
    std::printf("  HOPFIELD NETWORK DIAGNOSTICS\n");
    std::printf("  DIM=%zu  N=%zu  default reach=DIM/2=%zu  default beta=4.0\n", DIM, N, DIM / 2);
    std::printf("============================================================\n");

    bool all_pass = true;
    all_pass &= NoiseRecall<DIM>().RunAndPrint();
    all_pass &= EnergyMonotonicity<DIM>().RunAndPrint();
    all_pass &= CapacityProbe<DIM>().RunAndPrint();
    all_pass &= OverlapMetrics<DIM>().RunAndPrint();

    std::printf("\n============================================================\n");
    std::printf("  OVERALL: %s\n", all_pass ? "ALL PASSED" : "SOME TESTS FAILED");
    std::printf("============================================================\n");

    return all_pass;
}

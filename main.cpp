// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 David Liptak

#include "diagnostics/Diagnostics.h"

int main()
{
    constexpr size_t DIM = 8;
    const bool pass = RunDiagnostics<DIM>();
    return pass ? 0 : 1;
}

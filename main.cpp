#include "diagnostics/Diagnostics.h"

int main()
{
    constexpr size_t DIM = 8;
    const bool pass = RunDiagnostics<DIM>();
    return pass ? 0 : 1;
}

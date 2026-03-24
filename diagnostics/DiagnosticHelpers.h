#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

/// Shared utilities for Hopfield network diagnostics.

template <size_t N>
static void GenerateRandomPattern(float* out, std::mt19937_64& rng)
{
    std::uniform_int_distribution<int> coin(0, 1);
    for (size_t i = 0; i < N; ++i)
        out[i] = coin(rng) ? 1.0f : -1.0f;
}

template <size_t N>
[[nodiscard]] static size_t CorruptPattern(const float* original, float* noisy,
                                           float noise_level, std::mt19937_64& rng)
{
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    std::copy(original, original + N, noisy);
    size_t flipped = 0;
    for (size_t i = 0; i < N; ++i)
    {
        if (prob(rng) < noise_level)
        {
            noisy[i] = -noisy[i];
            ++flipped;
        }
    }
    return flipped;
}

template <size_t N>
static float ComputeOverlap(const float* a, const float* b)
{
    float dot = 0.0f;
    for (size_t i = 0; i < N; ++i)
        dot += a[i] * b[i];
    return dot / static_cast<float>(N);
}

static inline const std::array<uint64_t, 3>& DiagSeeds()
{
    static constexpr std::array<uint64_t, 3> seeds = {42, 1042, 2042};
    return seeds;
}

/// Write a line to both stdout and an open FILE*.
static inline void Tee(FILE* f, const std::string& line)
{
    std::fputs(line.c_str(), stdout);
    if (f) std::fputs(line.c_str(), f);
}

/// Format a string using snprintf. Handles overflow by dynamic allocation.
template <typename... Args>
static std::string Fmt(const char* fmt, Args... args)
{
    char buf[512];
    const int n = std::snprintf(buf, sizeof(buf), fmt, args...);
    if (n < 0) return {};
    if (static_cast<size_t>(n) >= sizeof(buf))
    {
        std::string result(static_cast<size_t>(n), '\0');
        std::snprintf(result.data(), result.size() + 1, fmt, args...);
        return result;
    }
    return buf;
}

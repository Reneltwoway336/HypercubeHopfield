// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 David Liptak

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

/// Shared utilities for Hopfield network diagnostics.

/// Generate a random continuous-valued pattern in [-1, 1].
template <size_t N>
static void GenerateRandomPattern(float* out, std::mt19937_64& rng)
{
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (size_t i = 0; i < N; ++i)
        out[i] = dist(rng);
}

/// Corrupt a pattern by adding Gaussian noise.
/// noise_level is the standard deviation of the Gaussian.
template <size_t N>
static void CorruptPattern(const float* original, float* noisy,
                           float noise_level, std::mt19937_64& rng)
{
    std::normal_distribution<float> gauss(0.0f, noise_level);
    std::copy(original, original + N, noisy);
    for (size_t i = 0; i < N; ++i)
        noisy[i] += gauss(rng);
}

/// Cosine similarity: dot(a, b) / (||a|| * ||b||).
/// Scale-invariant, returns 1.0 for identical directions, 0.0 for orthogonal.
template <size_t N>
static float ComputeOverlap(const float* a, const float* b)
{
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (size_t i = 0; i < N; ++i)
    {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    const float denom = std::sqrt(norm_a * norm_b);
    return (denom > 1e-12f) ? dot / denom : 0.0f;
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

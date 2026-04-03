# HypercubeHopfield C++ SDK

Static C++ library for modern Hopfield associative memory on Boolean hypercube
graphs.

## Contents

- [What's in the SDK](#whats-in-the-sdk)
- [Building from source](#building-from-source)
- [Using the SDK](#using-the-sdk)
  - [CMake FetchContent (recommended)](#cmake-fetchcontent-recommended)
  - [Installed SDK (find_package)](#installed-sdk-find_package)
- [Minimal example](#minimal-example)
- [API Reference](#api-reference)
  - [Template parameter: DIM](#template-parameter-dim)
  - [Enums](#enums)
  - [RecallResult](#recallresult)
  - [HopfieldNetwork\<DIM\>](#hopfieldnetworkdim)
  - [IHopfieldNetwork (type-erased interface)](#ihopfieldnetwork-type-erased-interface)
  - [CreateHopfieldNetwork (runtime factory)](#createhopfieldnetwork-runtime-factory)
- [Dependencies](#dependencies)

## What's in the SDK

After installation, the SDK contains:

```
<prefix>/
  include/HypercubeHopfield/
    HopfieldNetwork.h          -- The public API (the only header consumers include)
  lib/
    libHypercubeHopfieldCore.a
  lib/cmake/HypercubeHopfield/
    HypercubeHopfieldConfig.cmake
    HypercubeHopfieldTargets.cmake
    HypercubeHopfieldConfigVersion.cmake
```

Consumers include `<HypercubeHopfield/HopfieldNetwork.h>` and link against
`HypercubeHopfield::HypercubeHopfieldCore`. The single header contains the
complete public API: template class, type-erased interface, and runtime factory.

## Building from source

Requirements: C++23 compiler (GCC 13+, Clang 17+, MSVC 2022+), CMake 4.1+.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /path/to/sdk
```

## Using the SDK

### CMake FetchContent (recommended)

The simplest way to use HypercubeHopfield in a CMake project. No installation,
no manual downloads -- CMake pulls the source from GitHub and builds it
alongside your project.

```cmake
cmake_minimum_required(VERSION 4.1)
project(MyApp)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)
FetchContent_Declare(
    HypercubeHopfield
    GIT_REPOSITORY https://github.com/dliptak001/HypercubeHopfield.git
    GIT_TAG        v0.1.0
)
FetchContent_MakeAvailable(HypercubeHopfield)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HypercubeHopfieldCore)
```

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Pin `GIT_TAG` to a release tag (e.g., `v0.1.0`) for reproducible builds.
Include paths are set automatically -- use `#include "HopfieldNetwork.h"`.

> **Note:** FetchContent exposes the project root as an include path, so the
> bare include works. The namespaced `<HypercubeHopfield/HopfieldNetwork.h>`
> form is available when using an installed SDK via `find_package`.

### Installed SDK (find_package)

If you prefer to install the library once and link against it:

```bash
# Build and install
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /path/to/sdk
```

```cmake
cmake_minimum_required(VERSION 4.1)
project(MyApp)

set(CMAKE_CXX_STANDARD 23)

find_package(HypercubeHopfield REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HypercubeHopfield::HypercubeHopfieldCore)
```

Configure with the SDK path:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/sdk
cmake --build build
```

## Minimal example

```cpp
#include "HopfieldNetwork.h"  // or <HypercubeHopfield/HopfieldNetwork.h> if installed
#include <cstdio>
#include <random>
#include <vector>

int main()
{
    constexpr size_t DIM = 8;
    constexpr size_t N = 1ULL << DIM;  // 256 neurons

    // Create network with runtime DIM selection
    auto net = CreateHopfieldNetwork(DIM, /*seed=*/42);

    // Generate and store a random pattern
    std::mt19937_64 rng(123);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> pattern(N);
    for (auto& v : pattern) v = dist(rng);
    net->StorePattern(pattern);

    // Corrupt the pattern with heavy noise
    std::vector<float> probe(pattern);
    std::normal_distribution<float> noise(0.0f, 2.0f);
    for (auto& v : probe) v += noise(rng);

    // Recall: the network recovers the stored pattern
    auto [steps, converged] = net->Recall(probe);
    std::printf("Converged: %s in %zu steps\n",
                converged ? "yes" : "no", steps);
    return 0;
}
```

For compile-time DIM selection (avoids virtual dispatch):

```cpp
auto net = HopfieldNetwork<8>::Create(/*seed=*/42);
```

---

## API Reference

### Template parameter: DIM

`DIM` is a compile-time template parameter controlling the hypercube dimension.
The network has N = 2^DIM neurons. The library provides explicit template
instantiations for DIM 4-16.

| DIM | Neurons | Typical use |
|-----|---------|-------------|
| 4-6 | 16-64   | Fast prototyping, unit tests |
| 7-8 | 128-256 | Standard workloads, demos |
| 9-12 | 512-4096 | High-capacity associative memory |
| 13-16 | 8192-65536 | Research, maximum capacity |

---

### Enums

#### `UpdateMode`

| Value | Description |
|-------|-------------|
| `Async` | Sequential random-order updates. Guaranteed energy descent. Not parallelizable. |
| `Sync` | Simultaneous double-buffered updates. Deterministic, GPU-portable. Default. |

---

### RecallResult

```cpp
struct RecallResult
{
    size_t steps;     // Number of update sweeps performed
    bool   converged; // True if the state stabilized within tolerance
};
```

Returned by `Recall()`. Supports structured bindings:

```cpp
auto [steps, converged] = net->Recall(state);
```

---

### HopfieldNetwork\<DIM\>

The core template class. Owns the network state, stored patterns, connection
masks, and internal thread pool.

#### Construction

```cpp
static std::unique_ptr<HopfieldNetwork> Create(
    uint64_t rng_seed,
    size_t   reach              = DIM / 2,
    float    beta               = 4.0f,
    float    neighbor_fraction  = 1.0f,
    float    tolerance          = 1e-6f);
```

Returns a `unique_ptr` to a new network. Parameters:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `rng_seed` | `uint64_t` | Required | Random seed for update-order permutations. Deterministic given the same seed. |
| `reach` | `size_t` | `DIM/2` | Hamming-ball radius for neighbor connectivity (1 to DIM). Higher = more connections, more capacity, slower per-sweep. |
| `beta` | `float` | `4.0` | Inverse temperature for softmax attention. Higher = sharper (more winner-take-all) retrieval. |
| `neighbor_fraction` | `float` | `1.0` | Fraction of the Hamming ball to use (0.0, 1.0]. Masks are sorted by distance (closest first) then truncated. |
| `tolerance` | `float` | `1e-6` | Convergence threshold. A sweep is stable when no vertex changes by more than this. |

**Throws:** `std::invalid_argument` if `reach` is outside [1, DIM], `beta` is
not positive, `neighbor_fraction` is outside (0.0, 1.0], or `tolerance` is
negative.

---

#### Core Operations

##### `StorePattern`

```cpp
void StorePattern(std::span<const float> pattern);
```

Store a pattern for later retrieval. Patterns are stored explicitly (not
collapsed into a weight matrix). Continuous-valued floats -- not restricted
to {-1, +1}.

**Parameters:**
- `pattern` -- Exactly `NumVertices()` floats.

**Throws:** `std::invalid_argument` if `pattern.size() != NumVertices()`.

---

##### `Recall`

```cpp
RecallResult Recall(std::span<float> state,
                    size_t max_steps = 100,
                    UpdateMode mode = UpdateMode::Sync);
```

Run update sweeps until convergence or `max_steps`. The state buffer is
modified in place -- on return it holds the recalled pattern.

**Parameters:**
- `state` -- In/out: `NumVertices()` floats. Modified in place.
- `max_steps` -- Maximum update sweeps before declaring non-convergence.
- `mode` -- `Sync` (default, deterministic) or `Async` (guaranteed energy descent).

**Returns:** `RecallResult` with sweep count and convergence flag.
Returns `{0, false}` if no patterns are stored.

**Throws:** `std::invalid_argument` if `state.size() != NumVertices()`.

**Notes:**
- Sync mode uses internal multithreading for large workloads (DIM >= ~12 with many patterns).
- Async mode is inherently sequential (data dependency between vertex updates).

---

##### `Energy`

```cpp
[[nodiscard]] std::optional<float> Energy(std::span<const float> state) const;
```

Compute the modern Hopfield energy for the given state.

**Returns:** Energy value, or `std::nullopt` if no patterns are stored.

**Throws:** `std::invalid_argument` if `state.size() != NumVertices()`.

---

#### Pattern Management

##### `NumPatterns`

```cpp
[[nodiscard]] size_t NumPatterns() const;
```

Number of currently stored patterns.

---

##### `GetPattern`

```cpp
[[nodiscard]] std::span<const float> GetPattern(size_t idx) const;
```

Read back a stored pattern by index.

**Throws:** `std::out_of_range` if `idx >= NumPatterns()`.

---

##### `PopPattern`

```cpp
void PopPattern();
```

Remove the most recently stored pattern.

**Throws:** `std::out_of_range` if `NumPatterns() == 0`.

---

##### `Clear`

```cpp
void Clear();
```

Remove all stored patterns and reset internal state.

---

#### Introspection

These accessors return construction parameters. Together with `GetPattern()`,
they provide everything needed for serialization and reconstruction.

| Method | Returns | Description |
|--------|---------|-------------|
| `Dim()` | `size_t` | Hypercube dimension (template parameter DIM). |
| `NumVertices()` | `size_t` | Number of neurons: 2^DIM. |
| `Seed()` | `uint64_t` | Original RNG seed passed at construction. |
| `Reach()` | `size_t` | Hamming-ball radius. |
| `Beta()` | `float` | Inverse temperature. |
| `NeighborFraction()` | `float` | Fraction of Hamming ball used. |
| `Tolerance()` | `float` | Convergence threshold. |

**Serialization round-trip:**

```cpp
// Save: record these values + all patterns
auto dim = net->Dim();
auto seed = net->Seed();
auto reach = net->Reach();
auto beta = net->Beta();
auto nf = net->NeighborFraction();
auto tol = net->Tolerance();
for (size_t i = 0; i < net->NumPatterns(); ++i)
    save(net->GetPattern(i));

// Restore
auto restored = CreateHopfieldNetwork(dim, seed, reach, beta, nf, tol);
for (auto& pat : saved_patterns)
    restored->StorePattern(pat);
```

---

### IHopfieldNetwork (type-erased interface)

```cpp
class IHopfieldNetwork
```

Abstract base class with the same methods as `HopfieldNetwork<DIM>`, accessed
through virtual dispatch. Use this when DIM is not known at compile time (e.g.,
SDK bindings, plugin systems, configuration-driven applications).

Obtained via `CreateHopfieldNetwork()` (below). A
`unique_ptr<HopfieldNetwork<DIM>>` converts implicitly to
`unique_ptr<IHopfieldNetwork>` via move.

**Thread safety:** Not thread-safe. A single instance must not be accessed
concurrently. Create separate instances for concurrent use.

---

### CreateHopfieldNetwork (runtime factory)

```cpp
std::unique_ptr<IHopfieldNetwork> CreateHopfieldNetwork(
    size_t   dim,
    uint64_t rng_seed,
    size_t   reach              = 0,
    float    beta               = 4.0f,
    float    neighbor_fraction  = 1.0f,
    float    tolerance          = 1e-6f);
```

Create a network with DIM chosen at runtime. Returns a type-erased
`IHopfieldNetwork` pointer. All methods are available through virtual dispatch.

**Parameters:**
- `dim` -- Hypercube dimension (4-16).
- `rng_seed` -- Random seed.
- `reach` -- Hamming-ball radius. Pass 0 for the default (dim/2).
- `beta`, `neighbor_fraction`, `tolerance` -- Same as `HopfieldNetwork<DIM>::Create()`.

**Throws:** `std::invalid_argument` if `dim` is outside [4, 16], or if any
parameter is out of valid range.

---

## Dependencies

No external dependencies beyond the C++ standard library.

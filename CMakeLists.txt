cmake_minimum_required(VERSION 4.1)
project(HypercubeHopfield)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Prefer -std=c++23 over GNU extensions

# Source collection via GLOB — new .cpp files in project root and readout/ are picked up automatically
file(GLOB SOURCES
        ${PROJECT_SOURCE_DIR}/*.cpp
        ${PROJECT_SOURCE_DIR}/readout/*.cpp
)

add_executable(HypercubeHopfield ${SOURCES})

target_include_directories(HypercubeHopfield PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/readout ${PROJECT_SOURCE_DIR}/diagnostics)

# ——— Compiler-specific OpenMP handling ———
if(MINGW)
    # MinGW requires -fopenmp for both compilation and linking to enable libgomp
    target_compile_options(HypercubeHopfield PRIVATE -fopenmp)
    target_link_options(HypercubeHopfield PRIVATE -fopenmp)
    # -fopenmp automatically defines _OPENMP for the preprocessor
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT MINGW)
    # Standard CMake OpenMP support works well on Linux/macOS GCC/Clang
    find_package(OpenMP QUIET)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(HypercubeHopfield OpenMP::OpenMP_CXX)
    endif()
elseif(MSVC)
    # MSVC has built-in OpenMP support via /openmp flag
    target_compile_options(HypercubeHopfield PRIVATE /openmp)
endif()

# ——— Optimization and warning flags ———
if(MSVC)
    target_compile_options(HypercubeHopfield PRIVATE
            /W4
            $<$<CONFIG:Release>:/O2 /fp:fast>
    )
else()
    target_compile_options(HypercubeHopfield PRIVATE
            $<$<CONFIG:Release>:
            -O3 -march=native -mtune=native -ffast-math
            >
            $<$<CONFIG:Debug>:
            -O0 -g
            >
            -Wall -Wextra -Wno-unknown-pragmas
    )
endif()

# Optional: Static linking of libgcc/libstdc++ for easier distribution on Windows
if(MINGW)
    target_link_options(HypercubeHopfield PRIVATE -static-libgcc -static-libstdc++)
endif()

# ——— Examples ———
# (uncomment as examples are added)
# file(GLOB READOUT_SOURCES ${PROJECT_SOURCE_DIR}/readout/*.cpp)
# set(CORE_SOURCES ${PROJECT_SOURCE_DIR}/HopfieldNetwork.cpp ${READOUT_SOURCES})
#
# add_executable(PatternRecall examples/PatternRecall.cpp ${CORE_SOURCES})
# target_include_directories(PatternRecall PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/readout)
# target_compile_options(PatternRecall PRIVATE $<$<CONFIG:Release>:-O3 -march=native -ffast-math> -Wall -Wextra -Wno-unknown-pragmas)

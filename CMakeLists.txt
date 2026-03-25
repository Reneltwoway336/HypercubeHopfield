cmake_minimum_required(VERSION 4.1)
project(HypercubeHopfield)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Prefer -std=c++23 over GNU extensions

# Source collection via GLOB -- new .cpp files in project root are picked up automatically
file(GLOB SOURCES
        ${PROJECT_SOURCE_DIR}/*.cpp
)

add_executable(HypercubeHopfield ${SOURCES})

target_include_directories(HypercubeHopfield PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/diagnostics)

# --- Compiler-specific OpenMP handling ---
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

# --- Optimization and warning flags ---
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

# --- Examples ---
# Each example is a standalone executable with its own main().
# They link against the same HopfieldNetwork.cpp as the main target.

set(EXAMPLE_COMMON_SOURCES ${PROJECT_SOURCE_DIR}/HopfieldNetwork.cpp)

file(GLOB EXAMPLE_SOURCES ${PROJECT_SOURCE_DIR}/examples/*.cpp)

foreach(EXAMPLE_SRC ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SRC} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SRC} ${EXAMPLE_COMMON_SOURCES})
    target_include_directories(${EXAMPLE_NAME} PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/diagnostics)

    if(MINGW)
        target_compile_options(${EXAMPLE_NAME} PRIVATE -fopenmp)
        target_link_options(${EXAMPLE_NAME} PRIVATE -fopenmp -static-libgcc -static-libstdc++)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT MINGW)
        find_package(OpenMP QUIET)
        if(OpenMP_CXX_FOUND)
            target_link_libraries(${EXAMPLE_NAME} OpenMP::OpenMP_CXX)
        endif()
    elseif(MSVC)
        target_compile_options(${EXAMPLE_NAME} PRIVATE /openmp)
    endif()

    if(MSVC)
        target_compile_options(${EXAMPLE_NAME} PRIVATE /W4 $<$<CONFIG:Release>:/O2 /fp:fast>)
    else()
        target_compile_options(${EXAMPLE_NAME} PRIVATE
            $<$<CONFIG:Release>:-O3 -march=native -mtune=native -ffast-math>
            $<$<CONFIG:Debug>:-O0 -g>
            -Wall -Wextra -Wno-unknown-pragmas
        )
    endif()
endforeach()

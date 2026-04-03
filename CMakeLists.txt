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
            -Wall -Wextra
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

    # Static runtime on MinGW for easier distribution
    if(MINGW)
        target_link_options(${EXAMPLE_NAME} PRIVATE -static-libgcc -static-libstdc++)
    endif()

    if(MSVC)
        target_compile_options(${EXAMPLE_NAME} PRIVATE /W4 $<$<CONFIG:Release>:/O2 /fp:fast>)
    else()
        target_compile_options(${EXAMPLE_NAME} PRIVATE
            $<$<CONFIG:Release>:-O3 -march=native -mtune=native -ffast-math>
            $<$<CONFIG:Debug>:-O0 -g>
            -Wall -Wextra
        )
    endif()
endforeach()

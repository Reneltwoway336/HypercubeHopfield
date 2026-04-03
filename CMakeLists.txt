cmake_minimum_required(VERSION 4.1)
project(HypercubeHopfield VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Prefer -std=c++23 over GNU extensions

# --- Static library: core Hopfield network ---
add_library(HypercubeHopfieldCore STATIC
    ${PROJECT_SOURCE_DIR}/HopfieldNetwork.cpp
)

# Public header -- the only header consumers include
set(PUBLIC_HEADERS ${PROJECT_SOURCE_DIR}/HopfieldNetwork.h)

# Include directories: project root for in-tree builds (FetchContent),
# install prefix include/HypercubeHopfield for installed consumers
target_include_directories(HypercubeHopfieldCore
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# --- Optimization and warning flags (library) ---
if(MSVC)
    target_compile_options(HypercubeHopfieldCore PRIVATE
        /W4
        $<$<CONFIG:Release>:/O2 /fp:fast>
    )
else()
    target_compile_options(HypercubeHopfieldCore PRIVATE
        $<$<CONFIG:Release>:-O3 -march=native -mtune=native -ffast-math>
        $<$<CONFIG:Debug>:-O0 -g>
        -Wall -Wextra
    )
endif()

if(MINGW)
    target_link_options(HypercubeHopfieldCore PRIVATE -static-libgcc -static-libstdc++)
endif()

# --- Executables: only when building standalone (not via FetchContent) ---
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    add_executable(HypercubeHopfield main.cpp)
    target_include_directories(HypercubeHopfield PRIVATE ${PROJECT_SOURCE_DIR}/diagnostics)

    file(GLOB EXAMPLE_SOURCES ${PROJECT_SOURCE_DIR}/examples/*.cpp)
    set(EXAMPLE_TARGETS)
    foreach(EXAMPLE_SRC ${EXAMPLE_SOURCES})
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_SRC} NAME_WE)
        add_executable(${EXAMPLE_NAME} ${EXAMPLE_SRC})
        list(APPEND EXAMPLE_TARGETS ${EXAMPLE_NAME})
    endforeach()

    set(EXE_TARGETS HypercubeHopfield ${EXAMPLE_TARGETS})

    foreach(tgt IN LISTS EXE_TARGETS)
        target_link_libraries(${tgt} PRIVATE HypercubeHopfieldCore)

        if(MSVC)
            target_compile_options(${tgt} PRIVATE
                /W4
                $<$<CONFIG:Release>:/O2 /fp:fast>
            )
        else()
            target_compile_options(${tgt} PRIVATE
                $<$<CONFIG:Release>:-O3 -march=native -mtune=native -ffast-math>
                $<$<CONFIG:Debug>:-O0 -g>
                -Wall -Wextra
            )
        endif()

        if(MINGW)
            target_link_options(${tgt} PRIVATE -static-libgcc -static-libstdc++)
        endif()
    endforeach()
endif()

# --- Install rules ---
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install the static library
install(TARGETS HypercubeHopfieldCore
    EXPORT HypercubeHopfieldTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install public headers into include/HypercubeHopfield/
install(FILES ${PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HypercubeHopfield
)

# Generate and install CMake package config
install(EXPORT HypercubeHopfieldTargets
    FILE HypercubeHopfieldTargets.cmake
    NAMESPACE HypercubeHopfield::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HypercubeHopfield
)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/HypercubeHopfieldConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/HypercubeHopfieldConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HypercubeHopfield
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/HypercubeHopfieldConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/HypercubeHopfieldConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/HypercubeHopfieldConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HypercubeHopfield
)

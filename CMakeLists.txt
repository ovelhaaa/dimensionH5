cmake_minimum_required(VERSION 3.10)
project(DimensionChorus VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Treat warnings as errors for a clean host build
add_compile_options(-Wall -Wextra -Werror)

# Define DSP Core Library
add_library(dimension_core STATIC
    core/dimension_chorus.c
    core/dimension_modes.c
    core/dsp_biquad.c
    core/dsp_interp.c
)
target_include_directories(dimension_core PUBLIC core)
target_link_libraries(dimension_core m) # link math library for host

# Enable testing
enable_testing()

# Add Tests Directory
add_subdirectory(tests)

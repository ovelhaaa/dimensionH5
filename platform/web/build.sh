#!/bin/bash
set -e

# Config
OUT_DIR="dist"
mkdir -p "$OUT_DIR"

# Source files (Core DSP + Web wrapper)
SOURCES=(
    "../../core/dimension_chorus.c"
    "../../core/dimension_modes.c"
    "../../core/dsp_biquad.c"
    "../../core/dsp_interp.c"
    "wasm_wrapper.c"
)

# Compiler flags
CFLAGS="-O3 -Wall -Wextra -I../../ -s WASM=1 -s ALLOW_MEMORY_GROWTH=1"

# We export all functions that have EMSCRIPTEN_KEEPALIVE, plus standard allocator functions.
# Note: Modern Emscripten handles KEEPALIVE without explicitly listing them here,
# but we add _malloc and _free just in case we need them from JS.
CFLAGS="$CFLAGS -s EXPORTED_FUNCTIONS=['_malloc','_free']"
CFLAGS="$CFLAGS -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue']"

# For ES modules (optional, but good for worklets)
# We will use MODULARIZE to allow us to instantiate it cleanly.
CFLAGS="$CFLAGS -s MODULARIZE=1 -s EXPORT_ES6=1 -s ENVIRONMENT=web,worker -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','HEAPF32']"

echo "Building WebAssembly target..."

if ! command -v emcc &> /dev/null
then
    echo "Emscripten compiler (emcc) not found. Please install emsdk or run via Docker."
    # Don't fail completely if running locally without emcc, just warn.
    # In CI, we will use an emscripten action.
else
    emcc "${SOURCES[@]}" $CFLAGS -o "$OUT_DIR/dimension_chorus.js"
    echo "WebAssembly build complete. Output in $OUT_DIR/"
fi

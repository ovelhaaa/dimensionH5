#include <stdint.h>
#include <stddef.h>
#include <emscripten.h>
#include "core/dimension_chorus.h"

static DimensionChorusState chorus;

EMSCRIPTEN_KEEPALIVE
void dimension_init() {
    DimensionChorus_Init(&chorus);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_mode(int mode) {
    if (mode < 0 || mode >= DIMENSION_MODE_COUNT) {
        mode = 0;
    }
    DimensionChorus_SetMode(&chorus, (DimensionMode)mode);
}

EMSCRIPTEN_KEEPALIVE
void dimension_reset() {
    DimensionChorus_Reset(&chorus);
}

// Emscripten doesn't expose raw array pointers easily to JS by default,
// so we allocate memory inside WASM and give JS the pointers.
static float in_buffer[DSP_BLOCK_SIZE];
static float out_buffer[DSP_BLOCK_SIZE * 2]; // Stereo

EMSCRIPTEN_KEEPALIVE
float* dimension_get_in_buffer() {
    return in_buffer;
}

EMSCRIPTEN_KEEPALIVE
float* dimension_get_out_buffer() {
    return out_buffer;
}

EMSCRIPTEN_KEEPALIVE
int dimension_get_block_size() {
    return DSP_BLOCK_SIZE;
}

// Process exactly DSP_BLOCK_SIZE frames.
EMSCRIPTEN_KEEPALIVE
void dimension_process() {
    DimensionChorus_ProcessBlock(&chorus, in_buffer, out_buffer, DSP_BLOCK_SIZE);
}

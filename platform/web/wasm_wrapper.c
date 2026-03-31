#include <stdint.h>
#include <stddef.h>
#include <emscripten.h>
#include "core/dimension_chorus.h"

static DimensionChorusState chorus;

EMSCRIPTEN_KEEPALIVE
void dimension_init() {
    DimensionChorus_Init(&chorus);
}

/**
 * Set the active chorus mode by index; clamps invalid indices to 0.
 * @param mode Index of the desired mode in the range [0, DIMENSION_MODE_COUNT - 1].
 */
EMSCRIPTEN_KEEPALIVE
void dimension_set_mode(int mode) {
    if (mode < 0 || mode >= DIMENSION_MODE_COUNT) {
        mode = 0;
    }
    DimensionChorus_SetMode(&chorus, (DimensionMode)mode);
}

/**
 * Set the global chorus selection mode.
 *
 * @param selMode Selection mode index where 0 selects the first mode and 1 selects the second; any other value is treated as 0.
 */
EMSCRIPTEN_KEEPALIVE
void dimension_set_selection_mode(int selMode) {
    if (selMode < 0 || selMode > 1) {
        selMode = 0;
    }
    DimensionChorus_SetSelectionMode(&chorus, (DimensionSelectionMode)selMode);
}

/**
 * Set the active mode mask for the global chorus processor.
 *
 * @param mask 4-bit mode bitmask where bit 0..3 enable the corresponding modes (values 0–15).
 *             Values outside the range 0–15 are treated as 0.
 */
EMSCRIPTEN_KEEPALIVE
void dimension_set_mode_mask(int mask) {
    // 4 bits for 4 modes (0-15)
    if (mask < 0 || mask > 15) {
        mask = 0;
    }
    DimensionChorus_SetModeMask(&chorus, (uint8_t)mask);
}

/**
 * Reset the global Dimension chorus processor to its initial state.
 *
 * Restores the internal static `chorus` instance used by the WASM API to its default, uninitialized configuration.
 */
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

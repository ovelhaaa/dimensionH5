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
void dimension_set_selection_mode(int selMode) {
    if (selMode < 0 || selMode > 1) {
        selMode = 0;
    }
    DimensionChorus_SetSelectionMode(&chorus, (DimensionSelectionMode)selMode);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_mode_mask(int mask) {
    // 4 bits for 4 modes (0-15)
    if (mask < 0 || mask > 15) {
        mask = 0;
    }
    DimensionChorus_SetModeMask(&chorus, (uint8_t)mask);
}

EMSCRIPTEN_KEEPALIVE
void dimension_reset() {
    DimensionChorus_Reset(&chorus);
}

EMSCRIPTEN_KEEPALIVE
void dimension_enable_custom_params(int enabled) {
    DimensionChorus_EnableCustomParams(&chorus, enabled);
}

EMSCRIPTEN_KEEPALIVE
void dimension_load_base_mode(int mode) {
    if (mode < 0 || mode >= DIMENSION_MODE_COUNT) {
        mode = 0;
    }
    DimensionChorus_LoadBaseModeToCustom(&chorus, (DimensionMode)mode);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_rate(float rateHz) {
    DimensionModeParams p = DimensionChorus_GetCustomParams(&chorus);
    p.rateHz = rateHz;
    DimensionChorus_SetCustomParams(&chorus, p);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_base_ms(float baseMs) {
    DimensionModeParams p = DimensionChorus_GetCustomParams(&chorus);
    p.baseMs = baseMs;
    DimensionChorus_SetCustomParams(&chorus, p);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_depth_ms(float depthMs) {
    DimensionModeParams p = DimensionChorus_GetCustomParams(&chorus);
    p.depthMs = depthMs;
    DimensionChorus_SetCustomParams(&chorus, p);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_main_wet(float mainWet) {
    DimensionModeParams p = DimensionChorus_GetCustomParams(&chorus);
    p.mainWet = mainWet;
    DimensionChorus_SetCustomParams(&chorus, p);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_cross_wet(float crossWet) {
    DimensionModeParams p = DimensionChorus_GetCustomParams(&chorus);
    p.crossWet = crossWet;
    DimensionChorus_SetCustomParams(&chorus, p);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_hpf_hz(int voice, float hz) {
    DimensionModeParams p = DimensionChorus_GetCustomParams(&chorus);
    if (voice == 1) p.hpf1Hz = hz;
    else if (voice == 2) p.hpf2Hz = hz;
    DimensionChorus_SetCustomParams(&chorus, p);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_lpf_hz(int voice, float hz) {
    DimensionModeParams p = DimensionChorus_GetCustomParams(&chorus);
    if (voice == 1) p.lpf1Hz = hz;
    else if (voice == 2) p.lpf2Hz = hz;
    DimensionChorus_SetCustomParams(&chorus, p);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_wet_gain(int voice, float gain) {
    DimensionModeParams p = DimensionChorus_GetCustomParams(&chorus);
    if (voice == 1) p.wet1Gain = gain;
    else if (voice == 2) p.wet2Gain = gain;
    DimensionChorus_SetCustomParams(&chorus, p);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_base_offset2_ms(float offsetMs) {
    DimensionModeParams p = DimensionChorus_GetCustomParams(&chorus);
    p.baseOffset2Ms = offsetMs;
    DimensionChorus_SetCustomParams(&chorus, p);
}

EMSCRIPTEN_KEEPALIVE
void dimension_set_depth2_scale(float scale) {
    DimensionModeParams p = DimensionChorus_GetCustomParams(&chorus);
    p.depth2Scale = scale;
    DimensionChorus_SetCustomParams(&chorus, p);
}

// Getters to reflect current preset into UI
EMSCRIPTEN_KEEPALIVE
float dimension_get_rate() { return DimensionChorus_GetCustomParams(&chorus).rateHz; }

EMSCRIPTEN_KEEPALIVE
float dimension_get_base_ms() { return DimensionChorus_GetCustomParams(&chorus).baseMs; }

EMSCRIPTEN_KEEPALIVE
float dimension_get_depth_ms() { return DimensionChorus_GetCustomParams(&chorus).depthMs; }

EMSCRIPTEN_KEEPALIVE
float dimension_get_main_wet() { return DimensionChorus_GetCustomParams(&chorus).mainWet; }

EMSCRIPTEN_KEEPALIVE
float dimension_get_cross_wet() { return DimensionChorus_GetCustomParams(&chorus).crossWet; }

EMSCRIPTEN_KEEPALIVE
float dimension_get_hpf_hz(int voice) {
    return voice == 1 ? DimensionChorus_GetCustomParams(&chorus).hpf1Hz : DimensionChorus_GetCustomParams(&chorus).hpf2Hz;
}

EMSCRIPTEN_KEEPALIVE
float dimension_get_lpf_hz(int voice) {
    return voice == 1 ? DimensionChorus_GetCustomParams(&chorus).lpf1Hz : DimensionChorus_GetCustomParams(&chorus).lpf2Hz;
}

EMSCRIPTEN_KEEPALIVE
float dimension_get_wet_gain(int voice) {
    return voice == 1 ? DimensionChorus_GetCustomParams(&chorus).wet1Gain : DimensionChorus_GetCustomParams(&chorus).wet2Gain;
}

EMSCRIPTEN_KEEPALIVE
float dimension_get_base_offset2_ms() { return DimensionChorus_GetCustomParams(&chorus).baseOffset2Ms; }

EMSCRIPTEN_KEEPALIVE
float dimension_get_depth2_scale() { return DimensionChorus_GetCustomParams(&chorus).depth2Scale; }

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

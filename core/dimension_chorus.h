#ifndef DIMENSION_CHORUS_H
#define DIMENSION_CHORUS_H

#include <stdint.h>
#include <stddef.h>
#include "dsp_common.h"
#include "dsp_biquad.h"
#include "dimension_modes.h"

/**
 * @brief Main state structure for the Dimension Chorus.
 * Caller should allocate statically or globally. No dynamic allocation is used internally.
 */
typedef struct {
    // Shared circular delay line buffer
    float delayBuffer[DELAY_BUFFER_SIZE];
    uint32_t writeIndex;

    // Target control parameters (for the chosen mode)
    float targetRate;
    float targetBaseMs;
    float targetDepth;
    float targetMainW;
    float targetCrossW;

    // Smoothed working parameters
    float rate;
    float baseMs;
    float depth;
    float mainW;
    float crossW;

    // Fixed dry gain
    float dryGain;

    // LFO State
    float phaseAcc;
    float lfoSmoothed;
    float lfoCoef;

    // Per-voice Filters
    BiquadDf2T wet1Hpf;
    BiquadDf2T wet1Lpf;
    BiquadDf2T wet2Hpf;
    BiquadDf2T wet2Lpf;

} DimensionChorusState;

/**
 * @brief Initialize the Dimension Chorus state.
 * @param s State pointer.
 */
void DimensionChorus_Init(DimensionChorusState* s);

/**
 * @brief Reset the state (clear buffers and filters, retain current mode settings).
 * @param s State pointer.
 */
void DimensionChorus_Reset(DimensionChorusState* s);

/**
 * @brief Switch mode, causing filters and targets to update.
 * @param s State pointer.
 * @param mode Target dimension mode (0-3).
 */
void DimensionChorus_SetMode(DimensionChorusState* s, DimensionMode mode);

/**
 * @brief Process an audio block. Mono in, Stereo out.
 *
 * @param s State pointer.
 * @param inMono Input block (size: blockSize).
 * @param outStereo Output block interleaved L-R (size: blockSize * 2).
 * @param blockSize Number of frames to process.
 */
void DimensionChorus_ProcessBlock(
    DimensionChorusState* s,
    const float* inMono,
    float* outStereo,
    size_t blockSize);

/**
 * @brief Process an audio block and expose internal signals for offline validation and debugging.
 * Mono in, Stereo out. Also outputs dry, wet1, wet2, and monoSum arrays.
 * This is an exact mirror of DimensionChorus_ProcessBlock but with extra outputs.
 * DO NOT use in production code on STM32 due to extra array writes.
 *
 * @param s State pointer.
 * @param inMono Input block (size: blockSize).
 * @param outStereo Output block interleaved L-R (size: blockSize * 2).
 * @param outDry Output block mono dry signal. Can be NULL.
 * @param outWet1 Output block mono wet 1 signal. Can be NULL.
 * @param outWet2 Output block mono wet 2 signal. Can be NULL.
 * @param outMonoSum Output block mono sum (L+R). Can be NULL.
 * @param blockSize Number of frames to process.
 */
void DimensionChorus_ProcessBlock_Inspect(
    DimensionChorusState* s,
    const float* inMono,
    float* outStereo,
    float* outDry,
    float* outWet1,
    float* outWet2,
    float* outMonoSum,
    size_t blockSize);

#endif // DIMENSION_CHORUS_H

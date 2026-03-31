#include "dimension_chorus.h"
#include "dsp_interp.h"
#include <string.h> // for memset
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define WET_HPF_Q    0.707f // Butterworth
#define WET_LPF_Q    0.707f

// Smoothing coeff calculated for control-rate block smoothing
// This could be tuned to give ~10ms transition
#define CONTROL_SMOOTH_COEF 0.08f

static inline float Dimension_LfoShape(float x)
{
    return x - 0.18f * x * x * x;
}

static inline float Dimension_WetSoftSat(float x)
{
    x *= 1.08f;
    return x / (1.0f + 0.18f * fabsf(x));
}

static inline float Dimension_OutputSafety(float x)
{
    if (x > 0.99f) return 0.99f;
    if (x < -0.99f) return -0.99f;
    return x;
}

/**
 * Control-rate parameter update.
 * Performed once per block.
 */
static inline void Dimension_UpdateControl(DimensionChorusState* s)
{
    const float k = CONTROL_SMOOTH_COEF;

    s->rate   += k * (s->targetRate   - s->rate);
    s->baseMs += k * (s->targetBaseMs - s->baseMs);
    s->depth  += k * (s->targetDepth  - s->depth);
    s->mainW  += k * (s->targetMainW  - s->mainW);
    s->crossW += k * (s->targetCrossW - s->crossW);
    s->baseOffset2Ms += k * (s->targetBaseOffset2Ms - s->baseOffset2Ms);
    s->depth2Scale   += k * (s->targetDepth2Scale   - s->depth2Scale);
    s->wet1Gain      += k * (s->targetWet1Gain      - s->wet1Gain);
    s->wet2Gain      += k * (s->targetWet2Gain      - s->wet2Gain);
}

/**
 * Initialize a DimensionChorusState to a known startup state.
 *
 * Sets reasonable defaults (selection mode, mode/mask, dry gain), clears the delay
 * buffer, computes default LFO smoothing coefficients, resets internal modulation
 * and filter state, applies the default mode's parameters, and copies target
 * parameter values into the current fields to avoid audible ramps on startup.
 *
 * @param s Pointer to the DimensionChorusState to initialize (must be valid).
 */
void DimensionChorus_Init(DimensionChorusState* s)
{
    // Clear buffer
    memset(s->delayBuffer, 0, sizeof(s->delayBuffer));
    s->writeIndex = 0;

    // Selection defaults
    s->selectionMode = DIMENSION_SELECTION_SINGLE;
    s->modeMask = 1; // Default: Mode 1 active
    s->mode = DIMENSION_MODE_1;

    // Fixed param
    s->dryGain = 0.84f;

    // Default LFO Filter Coef for 2Hz cutoff at sampling rate 48k
    // a_lfo = 1 - e^(-2*pi*fc/fs)
    s->lfoCoef = 1.0f - expf(-2.0f * (float)M_PI * 2.0f / DSP_SAMPLE_RATE);
    s->lfoCoef2 = 1.0f - expf(-2.0f * (float)M_PI * 1.7f / DSP_SAMPLE_RATE);

    DimensionChorus_Reset(s);

    // Initialize modes & filter coefficients
    DimensionChorus_SetMode(s, DIMENSION_MODE_1);

    // Force targets to current instantly to avoid ramp from 0 on powerup
    s->rate   = s->targetRate;
    s->baseMs = s->targetBaseMs;
    s->depth  = s->targetDepth;
    s->mainW  = s->targetMainW;
    s->crossW = s->targetCrossW;
    s->baseOffset2Ms = s->targetBaseOffset2Ms;
    s->depth2Scale = s->targetDepth2Scale;
    s->wet1Gain = s->targetWet1Gain;
    s->wet2Gain = s->targetWet2Gain;
}

/**
 * Reset the Dimension chorus state to an initial, silent condition.
 *
 * Clears the delay buffer and write index, zeroes modulation state (phase accumulator
 * and LFO smoothing states), and initializes the four wet-path biquad filters.
 *
 * @param s Pointer to the DimensionChorusState to reset.
 */
void DimensionChorus_Reset(DimensionChorusState* s)
{
    memset(s->delayBuffer, 0, sizeof(s->delayBuffer));
    s->writeIndex = 0;

    s->phaseAcc = 0.0f;
    s->lfo1Smoothed = 0.0f;
    s->lfo2Smoothed = 0.0f;

    Dsp_BiquadInit(&s->wet1Hpf);
    Dsp_BiquadInit(&s->wet1Lpf);
    Dsp_BiquadInit(&s->wet2Hpf);
    Dsp_BiquadInit(&s->wet2Lpf);
}

/**
 * Apply mode parameters to the chorus state as smoothing targets and recompute wet-path filters.
 *
 * Copies values from params into the state's target parameter fields and recalculates HPF/LPF
 * biquad coefficients for both wet paths using the cutoff frequencies contained in params.
 *
 * @param s State to update.
 * @param params Source mode parameters whose values are written into the state's target fields.
 */
static void DimensionChorus_ApplyParams(DimensionChorusState* s, DimensionModeParams params)
{
    s->targetRate   = params.rateHz;
    s->targetBaseMs = params.baseMs;
    s->targetDepth  = params.depthMs;
    s->targetMainW  = params.mainWet;
    s->targetCrossW = params.crossWet;
    s->targetBaseOffset2Ms = params.baseOffset2Ms;
    s->targetDepth2Scale = params.depth2Scale;
    s->targetWet1Gain = params.wet1Gain;
    s->targetWet2Gain = params.wet2Gain;

    Dsp_BiquadCalcHPF(&s->wet1Hpf, params.hpf1Hz, WET_HPF_Q);
    Dsp_BiquadCalcLPF(&s->wet1Lpf, params.lpf1Hz, WET_LPF_Q);

    Dsp_BiquadCalcHPF(&s->wet2Hpf, params.hpf2Hz, WET_HPF_Q);
    Dsp_BiquadCalcLPF(&s->wet2Lpf, params.lpf2Hz, WET_LPF_Q);
}

/**
 * Set the selection mode for the chorus and apply the corresponding mode parameters.
 *
 * When set to DIMENSION_SELECTION_SINGLE, the chorus applies parameters for the current
 * single mode. When set to the combo selection mode, the chorus applies parameters
 * derived from the current mode mask.
 *
 * @param s State object to modify.
 * @param selMode New selection mode to set.
 */
void DimensionChorus_SetSelectionMode(DimensionChorusState* s, DimensionSelectionMode selMode)
{
    s->selectionMode = selMode;

    if (selMode == DIMENSION_SELECTION_SINGLE) {
        DimensionModeParams params = DimensionMode_GetParams(s->mode);
        DimensionChorus_ApplyParams(s, params);
    } else {
        DimensionModeParams params = DimensionMode_GetComboParams(s->modeMask);
        DimensionChorus_ApplyParams(s, params);
    }
}

/**
 * Update the chorus state's mode mask and, when in combo selection mode,
 * immediately apply the parameters corresponding to the new mask.
 *
 * @param s Chorus state to update.
 * @param mask Bitmask whose set bits indicate which modes are active for combo selection.
 */
void DimensionChorus_SetModeMask(DimensionChorusState* s, uint8_t mask)
{
    s->modeMask = mask;
    if (s->selectionMode == DIMENSION_SELECTION_COMBO) {
        DimensionModeParams params = DimensionMode_GetComboParams(mask);
        DimensionChorus_ApplyParams(s, params);
    }
}

/**
 * Set the active chorus mode and apply the corresponding parameters based on the current selection mode.
 *
 * Updates s->mode and sets s->modeMask to (1 << mode). If s->selectionMode is DIMENSION_SELECTION_SINGLE,
 * the parameters for the single `mode` are applied immediately. Otherwise, combo parameters derived from
 * the updated modeMask are applied immediately.
 *
 * @param s Chorus state to update; its mode and modeMask are modified and target parameters may be updated.
 * @param mode Mode to select; also used to compute the new modeMask as (1 << mode).
 */
void DimensionChorus_SetMode(DimensionChorusState* s, DimensionMode mode)
{
    s->mode = mode;

    // In single mode, selecting a mode also applies it immediately.
    // We update the mask so if we switch to combo mode, it reflects the last selection.
    s->modeMask = (1 << mode);

    if (s->selectionMode == DIMENSION_SELECTION_SINGLE) {
        DimensionModeParams params = DimensionMode_GetParams(mode);
        DimensionChorus_ApplyParams(s, params);
    } else {
        // If we're in combo mode, changing the explicit single mode doesn't apply immediately,
        // but we might want to let the UI drive this via SetModeMask directly.
        // For backwards compatibility or simplified UI calls:
        DimensionModeParams params = DimensionMode_GetComboParams(s->modeMask);
        DimensionChorus_ApplyParams(s, params);
    }
}
/**
 * Process a block of mono input samples into a stereo chorus output, updating internal modulation
 * and delay state stored in the provided DimensionChorusState.
 *
 * Applies per-block control smoothing, LFO-driven delay modulation, Hermite-interpolated delay
 * reads, wet-path voicing (HPF → soft saturation → LPF), mid/side stereo recombination, and
 * output clamping.
 *
 * @param s Pointer to the chorus state; is read and updated (delay buffer, writeIndex, LFO state,
 *          and internal biquad states).
 * @param inMono Pointer to an array of `blockSize` mono input samples.
 * @param outStereo Pointer to an array for interleaved stereo output samples (length >= 2*blockSize).
 * @param blockSize Number of samples to process from `inMono` into `outStereo`.
 */
void DimensionChorus_ProcessBlock(
    DimensionChorusState* s,
    const float* inMono,
    float* outStereo,
    size_t blockSize)
{
    // Update control params (rate, baseMs, etc.) once per block
    Dimension_UpdateControl(s);

    // Convert milliseconds to sample lengths (based on native SR)
    const float samplesPerMs = DSP_SAMPLE_RATE / 1000.0f;
    const float baseSamps  = s->baseMs * samplesPerMs;
    const float depthSamps = s->depth  * samplesPerMs;
    const float baseOffset2Samps = s->baseOffset2Ms * samplesPerMs;
    const float ratePhaseInc = s->rate / DSP_SAMPLE_RATE;

    uint32_t wIdx = s->writeIndex;

    for (size_t i = 0; i < blockSize; i++) {
        const float dry = inMono[i];

        // 1. Write input to delay buffer
        s->delayBuffer[wIdx] = dry;

        // 2. Compute LFO and Smoothed Triangle
        s->phaseAcc += ratePhaseInc;
        if (s->phaseAcc >= 1.0f) {
            s->phaseAcc -= 1.0f;
        }

        // Generate rough triangle [-1, 1]
        float triRaw;
        if (s->phaseAcc < 0.5f) {
            triRaw = 4.0f * s->phaseAcc - 1.0f;
        } else {
            triRaw = 3.0f - 4.0f * s->phaseAcc;
        }

        // Low-pass smooth + slight shaping to hide geometric corners
        s->lfo1Smoothed += s->lfoCoef * (triRaw - s->lfo1Smoothed);
        s->lfo2Smoothed += s->lfoCoef2 * (triRaw - s->lfo2Smoothed);
        const float lfo1 = Dimension_LfoShape(s->lfo1Smoothed);
        const float lfo2 = Dimension_LfoShape(s->lfo2Smoothed);

        // Calculate actual delay taps
        const float tapTime1 = baseSamps + depthSamps * lfo1;
        const float tapTime2 = (baseSamps + baseOffset2Samps) - (depthSamps * s->depth2Scale * lfo2);

        // Calculate fractional read positions.
        // The delay is measured from the current writeIndex.
        // tapTime represents the delay in samples.
        // Example: If wIdx = 5, tapTime = 2.5, readPos = 2.5.

        float readPos1 = (float)wIdx - tapTime1;
        float readPos2 = (float)wIdx - tapTime2;

        // Handling negative float values correctly before truncation in Dsp_ReadHermite.
        // While bitwise masking in Hermite handles integers well, shifting the float
        // ensures the fractional part remains correctly oriented (positive fractional distance).
        if (readPos1 < 0.0f) readPos1 += (float)DELAY_BUFFER_SIZE;
        if (readPos2 < 0.0f) readPos2 += (float)DELAY_BUFFER_SIZE;

        // 3. Hermite Interpolated Read
        float wet1 = Dsp_ReadHermite(s->delayBuffer, readPos1);
        float wet2 = Dsp_ReadHermite(s->delayBuffer, readPos2);

        // 4. Voicing HPF + LPF
        wet1 = Dsp_BiquadProcess(&s->wet1Hpf, wet1);
        wet1 = Dimension_WetSoftSat(wet1);
        wet1 = Dsp_BiquadProcess(&s->wet1Lpf, wet1);
        wet1 *= s->wet1Gain;

        wet2 = Dsp_BiquadProcess(&s->wet2Hpf, wet2);
        wet2 = Dimension_WetSoftSat(wet2);
        wet2 = Dsp_BiquadProcess(&s->wet2Lpf, wet2);
        wet2 *= s->wet2Gain;

        // 5. Stereo M/S wet recombination to preserve center while widening sides
        const float wetMid = 0.5f * (wet1 + wet2);
        const float wetSide = 0.5f * (wet1 - wet2);
        float outL = s->dryGain * dry + s->mainW * wetMid + s->crossW * wetSide;
        float outR = s->dryGain * dry + s->mainW * wetMid - s->crossW * wetSide;

        // 6. Transparent safety clamp (should rarely engage in normal use)
        outStereo[2 * i + 0] = Dimension_OutputSafety(outL);
        outStereo[2 * i + 1] = Dimension_OutputSafety(outR);

        // 7. Advance write pointer
        wIdx = (wIdx + 1u) & DELAY_BUFFER_MASK;
    }

    s->writeIndex = wIdx;
}

void DimensionChorus_ProcessBlock_Inspect(
    DimensionChorusState* s,
    const float* inMono,
    float* outStereo,
    float* outDry,
    float* outWet1,
    float* outWet2,
    float* outMonoSum,
    size_t blockSize)
{
    // Update control params (rate, baseMs, etc.) once per block
    Dimension_UpdateControl(s);

    // Convert milliseconds to sample lengths (based on native SR)
    const float samplesPerMs = DSP_SAMPLE_RATE / 1000.0f;
    const float baseSamps  = s->baseMs * samplesPerMs;
    const float depthSamps = s->depth  * samplesPerMs;
    const float baseOffset2Samps = s->baseOffset2Ms * samplesPerMs;
    const float ratePhaseInc = s->rate / DSP_SAMPLE_RATE;

    uint32_t wIdx = s->writeIndex;

    for (size_t i = 0; i < blockSize; i++) {
        const float dry = inMono[i];

        // 1. Write input to delay buffer
        s->delayBuffer[wIdx] = dry;

        // 2. Compute LFO and Smoothed Triangle
        s->phaseAcc += ratePhaseInc;
        if (s->phaseAcc >= 1.0f) {
            s->phaseAcc -= 1.0f;
        }

        // Generate rough triangle [-1, 1]
        float triRaw;
        if (s->phaseAcc < 0.5f) {
            triRaw = 4.0f * s->phaseAcc - 1.0f;
        } else {
            triRaw = 3.0f - 4.0f * s->phaseAcc;
        }

        // Low-pass smooth + slight shaping to hide geometric corners
        s->lfo1Smoothed += s->lfoCoef * (triRaw - s->lfo1Smoothed);
        s->lfo2Smoothed += s->lfoCoef2 * (triRaw - s->lfo2Smoothed);
        const float lfo1 = Dimension_LfoShape(s->lfo1Smoothed);
        const float lfo2 = Dimension_LfoShape(s->lfo2Smoothed);

        // Calculate actual delay taps
        const float tapTime1 = baseSamps + depthSamps * lfo1;
        const float tapTime2 = (baseSamps + baseOffset2Samps) - (depthSamps * s->depth2Scale * lfo2);

        float readPos1 = (float)wIdx - tapTime1;
        float readPos2 = (float)wIdx - tapTime2;

        if (readPos1 < 0.0f) readPos1 += (float)DELAY_BUFFER_SIZE;
        if (readPos2 < 0.0f) readPos2 += (float)DELAY_BUFFER_SIZE;

        // 3. Hermite Interpolated Read
        float wet1 = Dsp_ReadHermite(s->delayBuffer, readPos1);
        float wet2 = Dsp_ReadHermite(s->delayBuffer, readPos2);

        // 4. Voicing HPF + LPF
        wet1 = Dsp_BiquadProcess(&s->wet1Hpf, wet1);
        wet1 = Dimension_WetSoftSat(wet1);
        wet1 = Dsp_BiquadProcess(&s->wet1Lpf, wet1);
        wet1 *= s->wet1Gain;

        wet2 = Dsp_BiquadProcess(&s->wet2Hpf, wet2);
        wet2 = Dimension_WetSoftSat(wet2);
        wet2 = Dsp_BiquadProcess(&s->wet2Lpf, wet2);
        wet2 *= s->wet2Gain;

        // 5. Stereo M/S wet recombination to preserve center while widening sides
        const float wetMid = 0.5f * (wet1 + wet2);
        const float wetSide = 0.5f * (wet1 - wet2);
        float outL = s->dryGain * dry + s->mainW * wetMid + s->crossW * wetSide;
        float outR = s->dryGain * dry + s->mainW * wetMid - s->crossW * wetSide;

        float clippedL = Dimension_OutputSafety(outL);
        float clippedR = Dimension_OutputSafety(outR);

        // 6. Output Assignment with Safety Soft Clip
        if (outStereo) {
            outStereo[2 * i + 0] = clippedL;
            outStereo[2 * i + 1] = clippedR;
        }

        // Inspection outputs
        if (outDry) outDry[i] = dry;
        if (outWet1) outWet1[i] = wet1;
        if (outWet2) outWet2[i] = wet2;
        if (outMonoSum) outMonoSum[i] = 0.5f * (clippedL + clippedR); // Simple sum (-6dB)

        // 7. Advance write pointer
        wIdx = (wIdx + 1u) & DELAY_BUFFER_MASK;
    }

    s->writeIndex = wIdx;
}

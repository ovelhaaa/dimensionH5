#include "dimension_chorus.h"
#include "dsp_math.h"
#include "dsp_interp.h"
#include <string.h> // for memset
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Fixed wet filter parameters based on v1 specification
#define WET_HPF_FREQ 120.0f
#define WET_HPF_Q    0.707f // Butterworth
#define WET_LPF_FREQ 6200.0f
#define WET_LPF_Q    0.707f

// Smoothing coeff calculated for control-rate block smoothing
// This could be tuned to give ~10ms transition
#define CONTROL_SMOOTH_COEF 0.08f

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
}

void DimensionChorus_Init(DimensionChorusState* s)
{
    // Clear buffer
    memset(s->delayBuffer, 0, sizeof(s->delayBuffer));
    s->writeIndex = 0;

    // Fixed param
    s->dryGain = 0.88f;

    // Default LFO Filter Coef for 2Hz cutoff at sampling rate 48k
    // a_lfo = 1 - e^(-2*pi*fc/fs)
    s->lfoCoef = 1.0f - expf(-2.0f * (float)M_PI * 2.0f / DSP_SAMPLE_RATE);

    DimensionChorus_Reset(s);

    // Initialize modes & filter coefficients
    DimensionChorus_SetMode(s, DIMENSION_MODE_1);

    // Force targets to current instantly to avoid ramp from 0 on powerup
    s->rate   = s->targetRate;
    s->baseMs = s->targetBaseMs;
    s->depth  = s->targetDepth;
    s->mainW  = s->targetMainW;
    s->crossW = s->targetCrossW;
}

void DimensionChorus_Reset(DimensionChorusState* s)
{
    memset(s->delayBuffer, 0, sizeof(s->delayBuffer));
    s->writeIndex = 0;

    s->phaseAcc = 0.0f;
    s->lfoSmoothed = 0.0f;

    Dsp_BiquadInit(&s->wet1Hpf);
    Dsp_BiquadInit(&s->wet1Lpf);
    Dsp_BiquadInit(&s->wet2Hpf);
    Dsp_BiquadInit(&s->wet2Lpf);
}

void DimensionChorus_SetMode(DimensionChorusState* s, DimensionMode mode)
{
    DimensionModeParams params = DimensionMode_GetParams(mode);

    s->targetRate   = params.rateHz;
    s->targetBaseMs = params.baseMs;
    s->targetDepth  = params.depthMs;
    s->targetMainW  = params.mainWet;
    s->targetCrossW = params.crossWet;

    // Recalculate filters (they are fixed freq right now, but best practice
    // to init them here on mode switch for future flexibility)
    Dsp_BiquadCalcHPF(&s->wet1Hpf, WET_HPF_FREQ, WET_HPF_Q);
    Dsp_BiquadCalcLPF(&s->wet1Lpf, WET_LPF_FREQ, WET_LPF_Q);

    Dsp_BiquadCalcHPF(&s->wet2Hpf, WET_HPF_FREQ, WET_HPF_Q);
    Dsp_BiquadCalcLPF(&s->wet2Lpf, WET_LPF_FREQ, WET_LPF_Q);
}

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

        // Low-pass smooth the triangle wave
        s->lfoSmoothed += s->lfoCoef * (triRaw - s->lfoSmoothed);

        // Calculate actual delay taps
        const float tapTime1 = baseSamps + depthSamps * s->lfoSmoothed;
        const float tapTime2 = baseSamps - depthSamps * s->lfoSmoothed;

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
        wet1 = Dsp_BiquadProcess(&s->wet1Lpf, wet1);

        wet2 = Dsp_BiquadProcess(&s->wet2Hpf, wet2);
        wet2 = Dsp_BiquadProcess(&s->wet2Lpf, wet2);

        // 5. Stereo Crossmix
        float outL = s->dryGain * dry + s->mainW * wet1 - s->crossW * wet2;
        float outR = s->dryGain * dry + s->mainW * wet2 - s->crossW * wet1;

        // 6. Output Assignment with Safety Soft Clip
        outStereo[2 * i + 0] = Dsp_SoftClip(outL);
        outStereo[2 * i + 1] = Dsp_SoftClip(outR);

        // 7. Advance write pointer
        wIdx = (wIdx + 1u) & DELAY_BUFFER_MASK;
    }

    s->writeIndex = wIdx;
}

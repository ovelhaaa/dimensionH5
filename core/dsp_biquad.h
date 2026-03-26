#ifndef DSP_BIQUAD_H
#define DSP_BIQUAD_H

#include <stdint.h>
#include "dsp_common.h"

/**
 * @brief Direct Form II Transposed Biquad structure.
 */
typedef struct {
    float b0, b1, b2; // Feedforward coefficients
    float a1, a2;     // Feedback coefficients (denominator)
    float z1, z2;     // State memory
} BiquadDf2T;

/**
 * @brief Initialize Biquad state array to zero.
 */
void Dsp_BiquadInit(BiquadDf2T* bq);

/**
 * @brief Calculate coefficients for a High-Pass Filter (HPF).
 * Uses <math.h> internally, expected to be called only on mode switch or init.
 *
 * @param bq Pointer to biquad structure.
 * @param fc Cutoff frequency in Hz.
 * @param q Q factor.
 */
void Dsp_BiquadCalcHPF(BiquadDf2T* bq, float fc, float q);

/**
 * @brief Calculate coefficients for a Low-Pass Filter (LPF).
 * Uses <math.h> internally, expected to be called only on mode switch or init.
 *
 * @param bq Pointer to biquad structure.
 * @param fc Cutoff frequency in Hz.
 * @param q Q factor.
 */
void Dsp_BiquadCalcLPF(BiquadDf2T* bq, float fc, float q);

/**
 * @brief Process one sample through the Biquad filter (DF2T).
 * Intended for the hot path. Uses no <math.h>.
 *
 * @param bq Pointer to the biquad structure.
 * @param in Input sample.
 * @return Processed output sample.
 */
static inline float Dsp_BiquadProcess(BiquadDf2T* bq, float in)
{
    float out = (in * bq->b0) + bq->z1;

    // Update states (DF2T)
    bq->z1 = (in * bq->b1) - (out * bq->a1) + bq->z2;
    bq->z2 = (in * bq->b2) - (out * bq->a2);

    return out;
}

#endif // DSP_BIQUAD_H

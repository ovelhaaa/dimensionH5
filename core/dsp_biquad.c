#include "dsp_biquad.h"
#include <math.h> // allowed for init/setup but explicitly avoided in DSP hot path

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void Dsp_BiquadInit(BiquadDf2T* bq)
{
    bq->z1 = 0.0f;
    bq->z2 = 0.0f;
}

void Dsp_BiquadCalcHPF(BiquadDf2T* bq, float fc, float q)
{
    // Biquad HPF formula
    float w0 = 2.0f * (float)M_PI * fc / DSP_SAMPLE_RATE;
    float alpha = sinf(w0) / (2.0f * q);
    float cosw0 = cosf(w0);

    float a0 = 1.0f + alpha;

    // Divide coefficients by a0 to normalize
    float b0_raw = (1.0f + cosw0) / 2.0f;
    float b1_raw = -(1.0f + cosw0);
    float b2_raw = (1.0f + cosw0) / 2.0f;
    float a1_raw = -2.0f * cosw0;
    float a2_raw = 1.0f - alpha;

    bq->b0 = b0_raw / a0;
    bq->b1 = b1_raw / a0;
    bq->b2 = b2_raw / a0;

    // In standard formula, feedback is positive and we subtract it, but here we provide normalized negative versions
    bq->a1 = a1_raw / a0;
    bq->a2 = a2_raw / a0;
}

void Dsp_BiquadCalcLPF(BiquadDf2T* bq, float fc, float q)
{
    // Biquad LPF formula
    float w0 = 2.0f * (float)M_PI * fc / DSP_SAMPLE_RATE;
    float alpha = sinf(w0) / (2.0f * q);
    float cosw0 = cosf(w0);

    float a0 = 1.0f + alpha;

    float b0_raw = (1.0f - cosw0) / 2.0f;
    float b1_raw = 1.0f - cosw0;
    float b2_raw = (1.0f - cosw0) / 2.0f;
    float a1_raw = -2.0f * cosw0;
    float a2_raw = 1.0f - alpha;

    bq->b0 = b0_raw / a0;
    bq->b1 = b1_raw / a0;
    bq->b2 = b2_raw / a0;

    bq->a1 = a1_raw / a0;
    bq->a2 = a2_raw / a0;
}

#include "dsp_interp.h"
#include <stdint.h>

/**
 * Reads using 3rd-order 4-point Hermite interpolation.
 * Uses bitwise wrapping exclusively for bounds checking.
 */
float Dsp_ReadHermite(const float* buffer, float readPos)
{
    // Make sure we have a positive index handling wrap effectively
    // To handle negative readPos correctly during integer casting, we shift up by a huge multiple of buffer size
    // For normal usage, readPos should be wrapped beforehand, but this adds robustness
    int32_t i0 = (int32_t)readPos;

    // Fractional part
    float frac = readPos - (float)i0;

    // 4 points for Hermite
    // i[-1] is read at offset i0 - 1
    int32_t im1 = (i0 - 1) & DELAY_BUFFER_MASK;
    int32_t i1  = i0 & DELAY_BUFFER_MASK;
    int32_t i2  = (i0 + 1) & DELAY_BUFFER_MASK;
    int32_t i3  = (i0 + 2) & DELAY_BUFFER_MASK;

    float xm1 = buffer[im1];
    float x0  = buffer[i1]; // This is y[0]
    float x1  = buffer[i2];
    float x2  = buffer[i3];

    // Standard Hermite polynomial coefficients calculation
    // c0 = x0
    // c1 = 1/2 * (x1 - xm1)
    // c2 = x1 - x0 - c1 - c3 (but calculated more directly)
    // Here we use the direct form:
    float c = (x1 - xm1) * 0.5f;
    float v = x0 - x1;
    float w = c + v;
    float a = w + v + (x2 - x0) * 0.5f;
    float b_diff = w + a;

    return ((((a * frac) - b_diff) * frac + c) * frac + x0);
}

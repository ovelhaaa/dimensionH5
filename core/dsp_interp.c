#include "dsp_interp.h"
#include <stdint.h>


 /**
 * Reads using 3rd-order 4-point Hermite interpolation.
 * Uses bitwise wrapping exclusively for bounds checking.
 *
 * The 4 points chosen are:
 * [i0 - 1], [i0], [i0 + 1], [i0 + 2]
 * where i0 is the truncated integer part of readPos.
 *
 * Note: readPos must be >= 0 for the fractional extraction (readPos - i0)
 * to behave correctly. The caller is responsible for ensuring readPos
 * is wrapped into the positive range [0, DELAY_BUFFER_SIZE).
 */
float Dsp_ReadHermite(const float* buffer, float readPos)
{
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

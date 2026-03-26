#ifndef DSP_MATH_H
#define DSP_MATH_H

#include <stdint.h>

/**
 * @brief Fast DSP mathematical utilities for hot-path.
 * Contains no <math.h> dependencies.
 */

/**
 * @brief Safety soft-clipper for the final output stage.
 *
 * Uses a polynomial approximation: x - x^3 / 3 for -1 <= x <= 1.
 * Hard clamp beyond this range to ensure stability.
 *
 * @param x Input float value
 * @return Soft-clipped float value
 */
static inline float Dsp_SoftClip(float x)
{
    // Threshold where the curve meets the clamp
    const float threshold = 1.0f;

    if (x >= threshold) {
        return 2.0f / 3.0f;
    } else if (x <= -threshold) {
        return -2.0f / 3.0f;
    }

    // Polynomial soft-clip curve
    return x - (x * x * x) * (1.0f / 3.0f);
}

#endif // DSP_MATH_H

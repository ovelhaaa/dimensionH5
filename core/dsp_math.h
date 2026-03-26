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
 * Behavior/Curve:
 * - Uses a polynomial approximation: f(x) = x - (x^3 / 3) for x in [-1, 1].
 * - Linear/almost-linear range: For inputs near 0 (e.g., [-0.3, 0.3]), f(x) ~ x.
 *   At x = 0.5, output is 0.458 (minor compression).
 * - Transition point & Max effective output: At x = 1.0, the output peaks at 2/3 (0.666...).
 * - Out of bounds: Hard clamp at 2/3 for any input > 1.0 (or < -1.0).
 *
 * Note for V1:
 * This provides absolute safety to prevent wrap-around or distortion spikes before DAC,
 * but its hard maximum is bounded at 0.666... (or -3.5dBFS). If the application requires
 * true 0dBFS maximum peak output, scaling both the input and output by 1.5 would be necessary,
 * but for this Dimension Chorus V1, maintaining headroom is preferred.
 *
 * @param x Input float value
 * @return Soft-clipped float value
 */
static inline float Dsp_SoftClip(float x)
{
    const float threshold = 1.0f;

    if (x >= threshold) {
        return 0.666666667f; // 2/3
    } else if (x <= -threshold) {
        return -0.666666667f; // -2/3
    }

    return x - (x * x * x) * (1.0f / 3.0f);
}

#endif // DSP_MATH_H

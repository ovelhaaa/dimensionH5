#ifndef DSP_INTERP_H
#define DSP_INTERP_H

#include <stdint.h>
#include "dsp_common.h"

/**
 * @brief Reads a fractional sample from a circular buffer using Hermite 3rd-order interpolation.
 *
 * @param buffer Floating-point delay buffer.
 * @param readPos The fractional read position (absolute index).
 * @return Interpolated float sample.
 */
float Dsp_ReadHermite(const float* buffer, float readPos);

#endif // DSP_INTERP_H

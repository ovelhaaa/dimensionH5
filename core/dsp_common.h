#ifndef DSP_COMMON_H
#define DSP_COMMON_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Common DSP constants for Dimension Chorus v1
 */

// Native sampling rate
#define DSP_SAMPLE_RATE 48000.0f

// Delay buffer configuration
// 1024 samples (must be a power of 2 for bitwise wrap)
#define DELAY_BUFFER_SIZE 1024
#define DELAY_BUFFER_MASK (DELAY_BUFFER_SIZE - 1)

// Static assertion to ensure DELAY_BUFFER_SIZE is a power of 2
_Static_assert((DELAY_BUFFER_SIZE & DELAY_BUFFER_MASK) == 0,
               "DELAY_BUFFER_SIZE must be a power of 2 for bitwise wrapping to work.");

// Expected processing block size
#define DSP_BLOCK_SIZE 32

#endif // DSP_COMMON_H

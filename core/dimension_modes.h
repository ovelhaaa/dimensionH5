#ifndef DIMENSION_MODES_H
#define DIMENSION_MODES_H

#include <stdint.h>

/**
 * @brief Enum defining the 4 preset modes.
 */
typedef enum {
    DIMENSION_MODE_1 = 0,
    DIMENSION_MODE_2,
    DIMENSION_MODE_3,
    DIMENSION_MODE_4,
    DIMENSION_MODE_COUNT
} DimensionMode;

/**
 * @brief Enum defining the selection behavior (Authentic vs Combo).
 */
typedef enum {
    DIMENSION_SELECTION_SINGLE = 0,
    DIMENSION_SELECTION_COMBO
} DimensionSelectionMode;

/**
 * @brief Struct to hold the base parameters for a single mode.
 */
typedef struct {
    float rateHz;     // LFO Rate in Hz
    float baseMs;     // Base delay in milliseconds
    float depthMs;    // Modulation depth in milliseconds
    float mainWet;    // Main wet gain
    float crossWet;   // Cross wet gain
    float hpf1Hz;     // Wet voice A HPF frequency
    float lpf1Hz;     // Wet voice A LPF frequency
    float hpf2Hz;     // Wet voice B HPF frequency
    float lpf2Hz;     // Wet voice B LPF frequency
    float wet1Gain;   // Wet voice A static gain trim
    float wet2Gain;   // Wet voice B static gain trim
    float baseOffset2Ms; // Voice B static delay offset
    float depth2Scale;   // Voice B depth scaling
} DimensionModeParams;

/**
 * @brief Retrieve parameters for a given mode.
 *
 * @param mode The desired mode.
 * @return DimensionModeParams The corresponding parameters.
 */
DimensionModeParams DimensionMode_GetParams(DimensionMode mode);

/**
 * @brief Retrieve mixed parameters for a combination of modes.
 *
 * @param modeMask Bitmask of active modes (bit 0 = Mode 1, bit 1 = Mode 2, etc.).
 * @return DimensionModeParams The combined parameters.
 */
DimensionModeParams DimensionMode_GetComboParams(uint8_t modeMask);

#endif // DIMENSION_MODES_H

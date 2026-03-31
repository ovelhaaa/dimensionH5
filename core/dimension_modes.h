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
 * @brief Enum defining selection behavior.
 */
typedef enum {
    DIMENSION_SELECTION_SINGLE = 0, // Classic/Authentic mode
    DIMENSION_SELECTION_COMBO       // Expanded mode (allows stacking)
} DimensionSelectionMode;

/**
 * @brief Mask representing active modes.
 * Bit 0 = MODE 1, Bit 1 = MODE 2, etc.
 */
typedef uint8_t DimensionModeMask;

/**
 * @brief Struct to hold the base parameters for a single mode.
 */
typedef struct {
    float rateHz;     // LFO Rate in Hz
    float baseMs;     // Base delay in milliseconds
    float depthMs;    // Modulation depth in milliseconds
    float mainWet;    // Main wet gain
    float crossWet;   // Cross wet gain
} DimensionModeParams;

/**
 * @brief Retrieve parameters for a given mode.
 *
 * @param mode The desired mode.
 * @return DimensionModeParams The corresponding parameters.
 */
DimensionModeParams DimensionMode_GetParams(DimensionMode mode);

/**
 * @brief Resolves parameters based on current selection behavior and active modes mask.
 *
 * @param selMode Single or Combo mode.
 * @param mask Mask of active modes.
 * @return DimensionModeParams The resolved, combined parameters.
 */
DimensionModeParams DimensionMode_ResolveParams(DimensionSelectionMode selMode, DimensionModeMask mask);

#endif // DIMENSION_MODES_H

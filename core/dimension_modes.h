#ifndef DIMENSION_MODES_H
#define DIMENSION_MODES_H

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

#endif // DIMENSION_MODES_H

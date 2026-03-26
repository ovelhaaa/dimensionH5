#include "dimension_modes.h"

// Define the 4 preset modes.
// Fallback array handles bounds safely.
static const DimensionModeParams modeParams[DIMENSION_MODE_COUNT] = {
    // Mode 1: rate=0.18, base=10.5, depth=0.70, mainWet=0.20, crossWet=0.06
    { 0.18f, 10.5f, 0.70f, 0.20f, 0.06f },

    // Mode 2: rate=0.32, base=11.5, depth=1.00, mainWet=0.26, crossWet=0.09
    { 0.32f, 11.5f, 1.00f, 0.26f, 0.09f },

    // Mode 3: rate=0.55, base=12.5, depth=1.30, mainWet=0.32, crossWet=0.12
    { 0.55f, 12.5f, 1.30f, 0.32f, 0.12f },

    // Mode 4: rate=0.72, base=13.0, depth=1.45, mainWet=0.36, crossWet=0.15
    { 0.72f, 13.0f, 1.45f, 0.36f, 0.15f }
};

DimensionModeParams DimensionMode_GetParams(DimensionMode mode)
{
    if (mode < 0 || mode >= DIMENSION_MODE_COUNT) {
        mode = DIMENSION_MODE_1; // Safe fallback
    }

    return modeParams[mode];
}

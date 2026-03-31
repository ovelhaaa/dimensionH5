#include "dimension_modes.h"

// Define the 4 preset modes.
// Fallback array handles bounds safely.
static const DimensionModeParams modeParams[DIMENSION_MODE_COUNT] = {
    // Mode 1: near-always-on widening
    { 0.16f,  9.9f, 0.42f, 0.16f, 0.10f, 145.0f, 5100.0f, 190.0f, 4300.0f, 1.00f, 0.92f, 0.12f, 0.92f },

    // Mode 2: more halo, still refined
    { 0.23f, 10.4f, 0.58f, 0.20f, 0.13f, 145.0f, 5000.0f, 190.0f, 4200.0f, 1.00f, 0.92f, 0.12f, 0.92f },

    // Mode 3: strongest record-ready sweet spot
    { 0.34f, 10.9f, 0.78f, 0.24f, 0.16f, 145.0f, 5100.0f, 190.0f, 4300.0f, 1.00f, 0.93f, 0.14f, 0.91f },

    // Mode 4: widest mode without obvious pop chorus
    { 0.48f, 11.6f, 0.98f, 0.28f, 0.19f, 150.0f, 5300.0f, 200.0f, 4500.0f, 1.00f, 0.94f, 0.16f, 0.90f }
};

DimensionModeParams DimensionMode_GetParams(DimensionMode mode)
{
    if (mode < 0 || mode >= DIMENSION_MODE_COUNT) {
        mode = DIMENSION_MODE_1; // Safe fallback
    }

    return modeParams[mode];
}

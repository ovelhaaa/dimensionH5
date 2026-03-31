#include "dimension_modes.h"
#include "dsp_math.h"
#include <math.h>

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

DimensionModeParams DimensionMode_ResolveParams(DimensionSelectionMode selMode, DimensionModeMask mask)
{
    // Ensure mask has at least one mode active (fallback to MODE_1 if empty)
    if (mask == 0) {
        mask = (1 << DIMENSION_MODE_1);
    }

    // Single mode: behavior is authentic. Choose highest bit/mode if multiple are somehow set.
    if (selMode == DIMENSION_SELECTION_SINGLE) {
        DimensionMode highestMode = DIMENSION_MODE_1;
        for (int i = DIMENSION_MODE_COUNT - 1; i >= 0; i--) {
            if (mask & (1 << i)) {
                highestMode = (DimensionMode)i;
                break;
            }
        }
        return DimensionMode_GetParams(highestMode);
    }

    // Combo mode processing
    int activeCount = 0;
    float sumRate = 0.0f;
    float sumBaseMs = 0.0f;
    float sumDepthMs = 0.0f;
    float sumMainWet = 0.0f;
    float sumCrossWet = 0.0f;
    float maxRate = 0.0f;

    for (int i = 0; i < DIMENSION_MODE_COUNT; i++) {
        if (mask & (1 << i)) {
            const DimensionModeParams* p = &modeParams[i];
            sumRate += p->rateHz;
            sumBaseMs += p->baseMs;
            sumDepthMs += p->depthMs;
            sumMainWet += p->mainWet;
            sumCrossWet += p->crossWet;
            if (p->rateHz > maxRate) {
                maxRate = p->rateHz;
            }
            activeCount++;
        }
    }

    // If only one mode is active, behavior matches Single Mode (no scaling)
    if (activeCount == 1) {
        return DimensionMode_GetParams((DimensionMode)(__builtin_ctz(mask))); // safe, mask != 0
    }

    DimensionModeParams finalParams;

    // COMBO STRATEGY
    // Base Delay: Weighted average (stable chorus core)
    finalParams.baseMs = sumBaseMs / (float)activeCount;

    // Rate: Weighted average of rates (prevents overly chaotic LFO, stays musical)
    finalParams.rateHz = sumRate / (float)activeCount;

    // Depth: Scaled sum, clamped to 1.5x max depth to prevent clipping the delay line
    float depthScale = 0.7f;
    finalParams.depthMs = sumDepthMs * depthScale;
    float maxAllowedDepth = modeParams[DIMENSION_MODE_4].depthMs * 1.5f;
    if (finalParams.depthMs > maxAllowedDepth) finalParams.depthMs = maxAllowedDepth;

    // Wet: Scaled sum, clamped
    float wetScale = 0.6f;
    finalParams.mainWet = sumMainWet * wetScale;
    finalParams.crossWet = sumCrossWet * wetScale;

    // Soft bounds for Wet to prevent total dry/wet imbalance or blowing up mix
    if (finalParams.mainWet > 0.6f) finalParams.mainWet = 0.6f;
    if (finalParams.crossWet > 0.3f) finalParams.crossWet = 0.3f;

    return finalParams;
}

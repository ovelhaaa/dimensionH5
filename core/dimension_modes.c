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

/**
 * Retrieve the preset DimensionModeParams for a specified DimensionMode.
 *
 * Validates `mode` against the range [0, DIMENSION_MODE_COUNT). If `mode` is out of range,
 * the function uses `DIMENSION_MODE_1` as a safe fallback and returns its preset.
 *
 * @param mode The requested DimensionMode (must be between 0 and DIMENSION_MODE_COUNT - 1).
 * @returns The preset DimensionModeParams corresponding to the (possibly adjusted) `mode`.
 */
DimensionModeParams DimensionMode_GetParams(DimensionMode mode)
{
    if (mode < 0 || mode >= DIMENSION_MODE_COUNT) {
        mode = DIMENSION_MODE_1; // Safe fallback
    }

    return modeParams[mode];
}

/**
 * Compute a combined DimensionModeParams from a bitmask of active modes.
 *
 * For a single active mode the exact preset is returned. For multiple active
 * modes the result mixes presets: `rateHz` is the maximum of selected rates;
 * `baseMs` and all filter/gain/secondary-delay fields are averaged;
 * `depthMs` is the sum of depths scaled by 0.7 and clamped to [0.2, 1.5];
 * `mainWet` is the sum of main wet values scaled by 0.6 and clamped to <= 0.4;
 * `crossWet` is the sum of cross wet values scaled by 0.6 and clamped to <= 0.3.
 *
 * @param modeMask Bitmask selecting modes; bit i enables mode i (0..DIMENSION_MODE_COUNT-1).
 *                 If zero or if no supported bits are set, the preset for DIMENSION_MODE_1 is returned.
 * @returns The combined DimensionModeParams computed from the selected modes.
 */
DimensionModeParams DimensionMode_GetComboParams(uint8_t modeMask)
{
    // If no mode selected or invalid mask, fallback to mode 1
    if (modeMask == 0 || (modeMask & ((1 << DIMENSION_MODE_COUNT) - 1)) == 0) {
        return DimensionMode_GetParams(DIMENSION_MODE_1);
    }

    // Check if only one mode is active (power of 2)
    // If so, just return its parameters exactly.
    if ((modeMask & (modeMask - 1)) == 0) {
        for (int i = 0; i < DIMENSION_MODE_COUNT; i++) {
            if (modeMask & (1 << i)) {
                return DimensionMode_GetParams((DimensionMode)i);
            }
        }
    }

    // Accumulators
    float sumRate = 0.0f;
    float sumBaseMs = 0.0f;
    float sumDepthMs = 0.0f;
    float sumMainWet = 0.0f;
    float sumCrossWet = 0.0f;
    float sumHpf1Hz = 0.0f;
    float sumLpf1Hz = 0.0f;
    float sumHpf2Hz = 0.0f;
    float sumLpf2Hz = 0.0f;
    float sumWet1Gain = 0.0f;
    float sumWet2Gain = 0.0f;
    float sumBaseOffset2Ms = 0.0f;
    float sumDepth2Scale = 0.0f;

    int activeCount = 0;

    for (int i = 0; i < DIMENSION_MODE_COUNT; i++) {
        if (modeMask & (1 << i)) {
            const DimensionModeParams* p = &modeParams[i];

            // Rate: keep max, or we can use weighted average.
            // A conservative weighted average gives a musical blend.
            // But max(rate) keeps the fastest movement, typical of pushing multiple buttons.
            if (p->rateHz > sumRate) sumRate = p->rateHz;

            sumBaseMs += p->baseMs;
            sumDepthMs += p->depthMs;
            sumMainWet += p->mainWet;
            sumCrossWet += p->crossWet;
            sumHpf1Hz += p->hpf1Hz;
            sumLpf1Hz += p->lpf1Hz;
            sumHpf2Hz += p->hpf2Hz;
            sumLpf2Hz += p->lpf2Hz;
            sumWet1Gain += p->wet1Gain;
            sumWet2Gain += p->wet2Gain;
            sumBaseOffset2Ms += p->baseOffset2Ms;
            sumDepth2Scale += p->depth2Scale;

            activeCount++;
        }
    }

    // Resolve final combo values
    DimensionModeParams result;

    // Scale factors
    const float depthScale = 0.7f;
    const float wetScale = 0.6f;
    const float crossScale = 0.6f;

    // Limits
    const float maxDepthMs = 1.5f;
    const float minDepthMs = 0.2f;
    const float maxWet = 0.4f;
    const float maxCross = 0.3f;

    // Rate: Use maximum observed rate
    result.rateHz = sumRate;

    // Base: Average
    result.baseMs = sumBaseMs / (float)activeCount;

    // Depth: Scaled sum, clamped
    result.depthMs = sumDepthMs * depthScale;
    if (result.depthMs > maxDepthMs) result.depthMs = maxDepthMs;
    if (result.depthMs < minDepthMs) result.depthMs = minDepthMs;

    // Wets: Scaled sum, clamped
    result.mainWet = sumMainWet * wetScale;
    if (result.mainWet > maxWet) result.mainWet = maxWet;

    result.crossWet = sumCrossWet * crossScale;
    if (result.crossWet > maxCross) result.crossWet = maxCross;

    // Filter cutoffs / Gains / Secondary Delays: Average
    result.hpf1Hz = sumHpf1Hz / (float)activeCount;
    result.lpf1Hz = sumLpf1Hz / (float)activeCount;
    result.hpf2Hz = sumHpf2Hz / (float)activeCount;
    result.lpf2Hz = sumLpf2Hz / (float)activeCount;
    result.wet1Gain = sumWet1Gain / (float)activeCount;
    result.wet2Gain = sumWet2Gain / (float)activeCount;
    result.baseOffset2Ms = sumBaseOffset2Ms / (float)activeCount;
    result.depth2Scale = sumDepth2Scale / (float)activeCount;

    return result;
}

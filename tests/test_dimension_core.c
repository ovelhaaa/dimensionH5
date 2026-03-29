#include <stdio.h>
#include <math.h>
#include "dimension_chorus.h"

int test_init_reset() {
    DimensionChorusState s;
    DimensionChorus_Init(&s);

    if (s.writeIndex != 0) return 1;
    if (s.dryGain != 0.88f) return 1;
    if (s.delayBuffer[0] != 0.0f) return 1;

    // Check if mode targets were loaded correctly
    if (s.rate != s.targetRate) return 1;

    return 0;
}

int test_set_mode() {
    DimensionChorusState s;
    DimensionChorus_Init(&s);

    // Test a specific mode switch
    DimensionChorus_SetMode(&s, DIMENSION_MODE_3);
    if (s.targetRate != 0.55f) return 1;
    if (s.targetBaseMs != 12.5f) return 1;

    return 0;
}

int test_process_block_stability() {
    DimensionChorusState s;
    DimensionChorus_Init(&s);

    float inBlock[32] = {0};
    float outBlock[64] = {0};

    // Feed silence
    DimensionChorus_ProcessBlock(&s, inBlock, outBlock, 32);

    for (int i = 0; i < 64; i++) {
        if (isnan(outBlock[i]) || isinf(outBlock[i])) {
            return 1;
        }
    }

    // Feed DC offset to check for filter explosions
    for (int i=0; i<32; i++) inBlock[i] = 1.0f;
    for (int b=0; b<100; b++) {
        DimensionChorus_ProcessBlock(&s, inBlock, outBlock, 32);
    }

    for (int i = 0; i < 64; i++) {
        if (isnan(outBlock[i]) || isinf(outBlock[i])) {
            return 1;
        }
    }

    return 0;
}

int test_impulse_response() {
    DimensionChorusState s;
    DimensionChorus_Init(&s);
    DimensionChorus_SetMode(&s, DIMENSION_MODE_1);

    // Disable LFO (depth = 0) to get deterministic static delay
    s.targetDepth = 0.0f;
    s.depth = 0.0f;

    float inBlock[32] = {0};
    float outBlock[64] = {0};

    // Feed impulse
    inBlock[0] = 1.0f;

    // Process a few blocks to capture the delay output
    // Base ms for Mode 1 is ~8ms. At 48kHz, that's 384 samples.
    // 384 samples / 32 block size = 12 blocks.
    // We should allow some time for the control rate parameters to smooth into the target state.
    // The target is 8ms, but baseMs starts at whatever init set it to, and moves towards target.
    // Since we forced s.baseMs = s.targetBaseMs in Init, it should be fast.

    int blockCount = 30; // increase blocks to wait for impulse to emerge through biquads
    int foundImpulse = 0;

    for (int b = 0; b < blockCount; b++) {
        DimensionChorus_ProcessBlock(&s, inBlock, outBlock, 32);

        // Check wet outputs - the biquad filters might delay/smear the impulse slightly
        for (int i = 0; i < 64; i++) {
            if (fabsf(outBlock[i]) > 0.001f && b > 0) { // lowered threshold as filter reduces impulse amp
                foundImpulse = 1;
            }
        }

        // Clear input after first block
        if (b == 0) inBlock[0] = 0.0f;
    }

    if (!foundImpulse) return 1;

    return 0;
}

int test_mono_compatibility() {
    DimensionChorusState s;
    DimensionChorus_Init(&s);

    float inBlock[32];
    float outBlock[64];

    // Generate simple sine wave for one block
    for(int i=0; i<32; i++) {
        inBlock[i] = sinf(2.0f * 3.14159f * 440.0f * (i / 48000.0f));
    }

    DimensionChorus_ProcessBlock(&s, inBlock, outBlock, 32);

    // Sum L+R
    for(int i=0; i<32; i++) {
        float monoSum = 0.5f * (outBlock[i*2] + outBlock[i*2+1]);
        if (isnan(monoSum) || isinf(monoSum)) {
            return 1;
        }
    }

    return 0;
}

int test_mode_switching() {
    DimensionChorusState s;
    DimensionChorus_Init(&s);

    float inBlock[32] = {0};
    float outBlock[64] = {0};

    for(int i=0; i<32; i++) inBlock[i] = 0.5f; // DC offset

    // Process while switching modes
    for (int m = DIMENSION_MODE_1; m <= DIMENSION_MODE_4; m++) {
        DimensionChorus_SetMode(&s, (DimensionMode)m);
        DimensionChorus_ProcessBlock(&s, inBlock, outBlock, 32);

        for (int i = 0; i < 64; i++) {
            if (isnan(outBlock[i]) || isinf(outBlock[i])) {
                return 1;
            }
        }
    }

    return 0;
}

int run_tests(void) {
    int failures = 0;

    printf("Running Core Tests...\n");
    if (test_init_reset() != 0) { printf("FAIL: test_init_reset\n"); failures++; }
    if (test_set_mode() != 0) { printf("FAIL: test_set_mode\n"); failures++; }
    if (test_process_block_stability() != 0) { printf("FAIL: test_process_block_stability\n"); failures++; }
    if (test_impulse_response() != 0) { printf("FAIL: test_impulse_response\n"); failures++; }
    if (test_mono_compatibility() != 0) { printf("FAIL: test_mono_compatibility\n"); failures++; }
    if (test_mode_switching() != 0) { printf("FAIL: test_mode_switching\n"); failures++; }

    return failures;
}

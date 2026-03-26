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

int run_tests(void) {
    int failures = 0;

    printf("Running Core Tests...\n");
    if (test_init_reset() != 0) { printf("FAIL: test_init_reset\n"); failures++; }
    if (test_set_mode() != 0) { printf("FAIL: test_set_mode\n"); failures++; }
    if (test_process_block_stability() != 0) { printf("FAIL: test_process_block_stability\n"); failures++; }

    return failures;
}

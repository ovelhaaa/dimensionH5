#include <stdio.h>
#include <math.h>
#include "dsp_interp.h"

int test_hermite_bounds() {
    float buffer[DELAY_BUFFER_SIZE] = {0};

    // Set some test values
    buffer[0] = 1.0f;
    buffer[1] = 2.0f;
    buffer[DELAY_BUFFER_SIZE - 1] = 3.0f;

    // Read at index 0 (fractional 0)
    float val = Dsp_ReadHermite(buffer, 0.0f);
    if (fabsf(val - 1.0f) > 1e-6) return 1;

    // Read at index 0 (wrap backwards to SIZE-1 for xm1)
    // Dsp_ReadHermite should correctly use DELAY_BUFFER_MASK to find xm1 at SIZE-1
    float val2 = Dsp_ReadHermite(buffer, 0.5f);
    // Should be smoothly interpolated between 1.0 and 2.0
    if (isnan(val2) || isinf(val2)) return 1;

    // Read near the end boundary
    float val3 = Dsp_ReadHermite(buffer, (float)(DELAY_BUFFER_SIZE - 1) + 0.5f);
    if (isnan(val3) || isinf(val3)) return 1;

    return 0;
}

int run_tests(void) {
    int failures = 0;
    printf("Running Interp Tests...\n");
    if (test_hermite_bounds() != 0) { printf("FAIL: test_hermite_bounds\n"); failures++; }
    return failures;
}

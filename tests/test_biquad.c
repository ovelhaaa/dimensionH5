#include <stdio.h>
#include <math.h>
#include "dsp_biquad.h"

int test_biquad_valid() {
    BiquadDf2T filter;
    Dsp_BiquadInit(&filter);

    // Calculate typical parameters from chorus
    Dsp_BiquadCalcHPF(&filter, 120.0f, 0.707f);

    if (isnan(filter.b0) || isinf(filter.b0)) return 1;
    if (isnan(filter.a1) || isinf(filter.a1)) return 1;

    // Process impulse
    float out = Dsp_BiquadProcess(&filter, 1.0f);
    if (isnan(out) || isinf(out)) return 1;

    // Process silence
    for(int i=0; i<10; i++) {
        out = Dsp_BiquadProcess(&filter, 0.0f);
        if (isnan(out) || isinf(out)) return 1;
    }

    return 0;
}

int run_tests(void) {
    int failures = 0;
    printf("Running Biquad Tests...\n");
    if (test_biquad_valid() != 0) { printf("FAIL: test_biquad_valid\n"); failures++; }
    return failures;
}

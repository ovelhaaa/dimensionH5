#include <stdio.h>
#include <assert.h>

// To test internal state, we include the .c file directly.
// We need to define some things that audio_engine.c expects but are platform-specific.
#define AUDIO_BLOCK_FRAMES 32
#define AUDIO_CHANNELS 2

#include "../app/audio_engine.c"

int test_audio_engine_set_mode() {
    printf("Running test_audio_engine_set_mode...\n");

    // Initialize engine
    AudioEngine_Init();

    // Test Mode 0 (DIMENSION_MODE_1)
    AudioEngine_SetMode(0);
    assert(chorus_state.targetRate == 0.18f);

    // Test Mode 1 (DIMENSION_MODE_2)
    AudioEngine_SetMode(1);
    assert(chorus_state.targetRate == 0.32f);

    // Test Mode 2 (DIMENSION_MODE_3)
    AudioEngine_SetMode(2);
    assert(chorus_state.targetRate == 0.55f);

    // Test Mode 3 (DIMENSION_MODE_4)
    AudioEngine_SetMode(3);
    assert(chorus_state.targetRate == 0.72f);

    // Test Out of bounds (Should default to DIMENSION_MODE_1)
    AudioEngine_SetMode(4);
    assert(chorus_state.targetRate == 0.18f);

    AudioEngine_SetMode(-1);
    assert(chorus_state.targetRate == 0.18f);

    printf("test_audio_engine_set_mode PASSED.\n");
    return 0;
}

int run_tests(void) {
    int failures = 0;
    if (test_audio_engine_set_mode() != 0) failures++;
    return failures;
}

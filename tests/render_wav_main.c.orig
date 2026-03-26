#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "dimension_chorus.h"
#include "wav_writer.h"

#define SAMPLE_RATE 48000
#define DURATION_SECONDS 5
#define BLOCK_SIZE 32
#define TOTAL_FRAMES (SAMPLE_RATE * DURATION_SECONDS)

// Generate a test signal: an impulse at the beginning, followed by silence,
// then a 440Hz sine wave to hear the chorusing
void generate_test_signal(float* inMono, size_t numFrames) {
    for (size_t i = 0; i < numFrames; i++) {
        inMono[i] = 0.0f;
    }

    // 1. Dirac impulse at frame 10 (gives it a small transient)
    inMono[10] = 1.0f;

    // 2. Sine wave starting at 1 second
    size_t startSine = SAMPLE_RATE * 1;
    for (size_t i = startSine; i < numFrames; i++) {
        float time = (float)(i - startSine) / SAMPLE_RATE;
        // Sawtooth sweep can reveal comb filtering better, but sine is simple. Let's do a sine sweep (chirp)
        // Frequency goes from 100Hz to 1kHz
        float f0 = 100.0f;
        float f1 = 1000.0f;
        float duration = 4.0f;
        float freq = f0 + (f1 - f0) * (time / duration);
        inMono[i] = 0.5f * sinf(2.0f * 3.14159265f * freq * time);
    }
}

int render_mode(DimensionMode mode, const char* filename, const float* inMono, size_t totalFrames) {
    DimensionChorusState state;
    DimensionChorus_Init(&state);
    DimensionChorus_SetMode(&state, mode);

    // Pre-allocate output buffer (interleaved stereo)
    float* outStereo = (float*)malloc(totalFrames * 2 * sizeof(float));
    if (!outStereo) {
        printf("Failed to allocate memory for %s\n", filename);
        return -1;
    }

    // Process block by block
    size_t framesProcessed = 0;
    while (framesProcessed < totalFrames) {
        size_t framesToProcess = BLOCK_SIZE;
        if (framesProcessed + BLOCK_SIZE > totalFrames) {
            framesToProcess = totalFrames - framesProcessed;
        }

        DimensionChorus_ProcessBlock(&state,
                                     &inMono[framesProcessed],
                                     &outStereo[framesProcessed * 2],
                                     framesToProcess);

        framesProcessed += framesToProcess;
    }

    // Write to WAV
    int result = write_wav_file(filename, outStereo, totalFrames, SAMPLE_RATE);
    free(outStereo);

    if (result == 0) {
        printf("Rendered %s\n", filename);
    } else {
        printf("Failed to write %s\n", filename);
    }

    return result;
}

int main() {
    printf("Generating test signals...\n");

    float* inMono = (float*)malloc(TOTAL_FRAMES * sizeof(float));
    if (!inMono) {
        printf("Failed to allocate input buffer\n");
        return 1;
    }

    generate_test_signal(inMono, TOTAL_FRAMES);

    // Save the dry signal for reference
    float* inStereoDry = (float*)malloc(TOTAL_FRAMES * 2 * sizeof(float));
    for (size_t i = 0; i < TOTAL_FRAMES; i++) {
        inStereoDry[i*2] = inMono[i];
        inStereoDry[i*2 + 1] = inMono[i];
    }
    write_wav_file("tests/output/dry_reference.wav", inStereoDry, TOTAL_FRAMES, SAMPLE_RATE);
    free(inStereoDry);

    // Render all 4 modes
    render_mode(DIMENSION_MODE_1, "tests/output/mode_1.wav", inMono, TOTAL_FRAMES);
    render_mode(DIMENSION_MODE_2, "tests/output/mode_2.wav", inMono, TOTAL_FRAMES);
    render_mode(DIMENSION_MODE_3, "tests/output/mode_3.wav", inMono, TOTAL_FRAMES);
    render_mode(DIMENSION_MODE_4, "tests/output/mode_4.wav", inMono, TOTAL_FRAMES);

    free(inMono);
    printf("Rendering complete.\n");
    return 0;
}

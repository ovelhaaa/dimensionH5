#include "test_signals.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void generate_sine(float* buffer, size_t numFrames, float freq, float sampleRate) {
    float phase = 0.0f;
    float phaseInc = 2.0f * (float)M_PI * freq / sampleRate;
    for (size_t i = 0; i < numFrames; i++) {
        buffer[i] = TEST_SIGNAL_AMPLITUDE * sinf(phase);
        phase = fmodf(phase + phaseInc, 2.0f * (float)M_PI);
    }
}

void generate_sweep(float* buffer, size_t numFrames, float sampleRate, float durationSeconds) {
    float phase = 0.0f;
    float f0 = TEST_SWEEP_FREQ_START;
    float f1 = TEST_SWEEP_FREQ_END;
    for (size_t i = 0; i < numFrames; i++) {
        float time = (float)i / sampleRate;
        float instantaneous_freq = f0 + (f1 - f0) * (time / durationSeconds);
        float dt = 1.0f / sampleRate;
        phase += 2.0f * (float)M_PI * instantaneous_freq * dt;
        phase = fmodf(phase, 2.0f * (float)M_PI);
        buffer[i] = TEST_SIGNAL_AMPLITUDE * sinf(phase);
    }
}

void generate_impulse(float* buffer, size_t numFrames) {
    for (size_t i = 0; i < numFrames; i++) {
        buffer[i] = 0.0f;
    }
    buffer[TEST_IMPULSE_OFFSET] = TEST_IMPULSE_AMPLITUDE;
}

void generate_chord(float* buffer, size_t numFrames, float sampleRate) {
    float phase220 = 0.0f, phase275 = 0.0f, phase330 = 0.0f;
    float inc220 = 2.0f * (float)M_PI * TEST_CHORD_FREQ_1 / sampleRate;
    float inc275 = 2.0f * (float)M_PI * TEST_CHORD_FREQ_2 / sampleRate;
    float inc330 = 2.0f * (float)M_PI * TEST_CHORD_FREQ_3 / sampleRate;

    for (size_t i = 0; i < numFrames; i++) {
        float val = sinf(phase220) + sinf(phase275) + sinf(phase330);
        buffer[i] = TEST_SIGNAL_AMPLITUDE * (val / 3.0f);
        phase220 = fmodf(phase220 + inc220, 2.0f * (float)M_PI);
        phase275 = fmodf(phase275 + inc275, 2.0f * (float)M_PI);
        phase330 = fmodf(phase330 + inc330, 2.0f * (float)M_PI);
    }
}

void generate_noise(float* buffer, size_t numFrames) {
    for (size_t i = 0; i < numFrames; i++) {
        float r = ((float)rand() / (float)(RAND_MAX)) * 2.0f - 1.0f;
        buffer[i] = TEST_NOISE_AMPLITUDE * r;
    }
}

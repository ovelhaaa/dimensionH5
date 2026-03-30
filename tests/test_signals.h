#ifndef TEST_SIGNALS_H
#define TEST_SIGNALS_H

#include <stddef.h>

// Signal Generation Parameters
#define TEST_SIGNAL_AMPLITUDE 0.8f
#define TEST_SWEEP_FREQ_START 100.0f
#define TEST_SWEEP_FREQ_END 2000.0f
#define TEST_IMPULSE_OFFSET 10
#define TEST_IMPULSE_AMPLITUDE 1.0f
#define TEST_CHORD_FREQ_1 220.0f
#define TEST_CHORD_FREQ_2 275.0f
#define TEST_CHORD_FREQ_3 330.0f
#define TEST_NOISE_AMPLITUDE 0.5f

void generate_sine(float* buffer, size_t numFrames, float freq, float sampleRate);
void generate_sweep(float* buffer, size_t numFrames, float sampleRate, float durationSeconds);
void generate_impulse(float* buffer, size_t numFrames);
void generate_chord(float* buffer, size_t numFrames, float sampleRate);
void generate_noise(float* buffer, size_t numFrames);

#endif // TEST_SIGNALS_H

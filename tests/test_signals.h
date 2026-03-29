#ifndef TEST_SIGNALS_H
#define TEST_SIGNALS_H

#include <stddef.h>

void generate_sine(float* buffer, size_t numFrames, float freq, float sampleRate);
void generate_sweep(float* buffer, size_t numFrames, float sampleRate, float durationSeconds);
void generate_impulse(float* buffer, size_t numFrames);
void generate_chord(float* buffer, size_t numFrames, float sampleRate);
void generate_noise(float* buffer, size_t numFrames);

#endif // TEST_SIGNALS_H

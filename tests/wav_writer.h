#ifndef WAV_WRITER_H
#define WAV_WRITER_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Saves interleaved stereo float audio (-1.0 to 1.0) to a 16-bit PCM WAV file.
 *
 * @param filename Output filename.
 * @param audioData Interleaved stereo float data.
 * @param numFrames Number of frames (1 frame = 2 samples).
 * @param sampleRate Sample rate (e.g., 48000).
 * @return 0 on success, non-zero on error.
 */
int write_wav_file(const char* filename, const float* audioData, size_t numFrames, uint32_t sampleRate);

#endif // WAV_WRITER_H

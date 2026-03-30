#ifndef WAV_READER_H
#define WAV_READER_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Reads a 16-bit PCM WAV file and converts it to a mono float buffer (-1.0 to 1.0).
 * If the file is stereo, it performs a downmix (L+R)/2 to mono.
 *
 * @param filename The path to the WAV file.
 * @param outMonoBuffer Pointer to a float pointer that will be dynamically allocated
 *                      to store the resulting mono audio data. The caller is responsible for free().
 * @param outNumFrames Pointer to store the total number of frames read.
 * @param outSampleRate Pointer to store the sample rate of the file.
 * @return 0 on success, non-zero on error (e.g., file not found, unsupported format).
 */
int read_wav_file_mono(const char* filename, float** outMonoBuffer, size_t* outNumFrames, uint32_t* outSampleRate);

#endif // WAV_READER_H

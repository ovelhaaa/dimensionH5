#include "wav_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char riff[4];
    uint32_t chunkSize;
    char wave[4];
    char fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} __attribute__((packed)) WavFmtChunk;

int read_wav_file_mono(const char* filename, float** outMonoBuffer, size_t* outNumFrames, uint32_t* outSampleRate) {
    if (!filename || !outMonoBuffer || !outNumFrames || !outSampleRate) return -1;

    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Could not open WAV file '%s'.\n", filename);
        return -1;
    }

    WavFmtChunk header;
    if (fread(&header, sizeof(WavFmtChunk), 1, file) != 1) {
        fclose(file);
        return -1;
    }

    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0 || strncmp(header.fmt, "fmt ", 4) != 0) {
        printf("Error: Invalid WAV file format.\n");
        fclose(file);
        return -1;
    }

    if (header.audioFormat != 1) { // PCM
        printf("Error: Only uncompressed PCM WAV files are supported.\n");
        fclose(file);
        return -1;
    }

    if (header.bitsPerSample != 16) {
        printf("Error: Only 16-bit PCM is supported. Detected %d-bit.\n", header.bitsPerSample);
        fclose(file);
        return -1;
    }

    // Skip extra bytes in the fmt chunk if it's larger than 16
    if (header.fmtSize > 16) {
        fseek(file, header.fmtSize - 16, SEEK_CUR);
    }

    // Read the data chunk header
    char chunkId[4];
    uint32_t chunkSize;
    int dataFound = 0;

    while (fread(chunkId, 1, 4, file) == 4) {
        if (fread(&chunkSize, 4, 1, file) != 1) break;
        if (strncmp(chunkId, "data", 4) == 0) {
            dataFound = 1;
            break;
        }
        fseek(file, chunkSize, SEEK_CUR);
    }

    if (!dataFound) {
        printf("Error: Could not find 'data' chunk in WAV file.\n");
        fclose(file);
        return -1;
    }

    size_t totalSamples = chunkSize / (header.bitsPerSample / 8);
    size_t numFrames = totalSamples / header.numChannels;

    int16_t* pcmData = (int16_t*)malloc(chunkSize);
    if (!pcmData) {
        printf("Error: Memory allocation failed for PCM data.\n");
        fclose(file);
        return -1;
    }

    if (fread(pcmData, sizeof(int16_t), totalSamples, file) != totalSamples) {
         printf("Warning: Could not read all PCM data from chunk.\n");
    }

    float* monoBuffer = (float*)malloc(numFrames * sizeof(float));
    if (!monoBuffer) {
        printf("Error: Memory allocation failed for float mono buffer.\n");
        free(pcmData);
        fclose(file);
        return -1;
    }

    // Convert 16-bit PCM to float [-1.0f, 1.0f] and downmix if necessary
    for (size_t i = 0; i < numFrames; i++) {
        if (header.numChannels == 1) {
            monoBuffer[i] = pcmData[i] / 32768.0f;
        } else if (header.numChannels == 2) {
            float l = pcmData[i * 2] / 32768.0f;
            float r = pcmData[i * 2 + 1] / 32768.0f;
            monoBuffer[i] = (l + r) * 0.5f;
        } else {
             // Unsupported channel count, just take the first channel
             monoBuffer[i] = pcmData[i * header.numChannels] / 32768.0f;
        }
    }

    *outMonoBuffer = monoBuffer;
    *outNumFrames = numFrames;
    *outSampleRate = header.sampleRate;

    free(pcmData);
    fclose(file);
    return 0;
}

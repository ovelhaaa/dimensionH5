#include "wav_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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
    char data[4];
    uint32_t dataSize;
} __attribute__((packed)) WavHeader;

int write_wav_file(const char* filename, const float* audioData, size_t numFrames, uint32_t sampleRate) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return -1;
    }

    uint16_t numChannels = 2;
    uint16_t bitsPerSample = 16;
    size_t totalSamples = numFrames * numChannels;
    uint32_t dataSizeBytes = totalSamples * (bitsPerSample / 8);

    WavHeader header;
    // Fill riff chunk
    header.riff[0] = 'R'; header.riff[1] = 'I'; header.riff[2] = 'F'; header.riff[3] = 'F';
    header.chunkSize = 36 + dataSizeBytes;
    header.wave[0] = 'W'; header.wave[1] = 'A'; header.wave[2] = 'V'; header.wave[3] = 'E';

    // Fill fmt chunk
    header.fmt[0] = 'f'; header.fmt[1] = 'm'; header.fmt[2] = 't'; header.fmt[3] = ' ';
    header.fmtSize = 16; // PCM
    header.audioFormat = 1; // PCM
    header.numChannels = numChannels;
    header.sampleRate = sampleRate;
    header.byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    header.blockAlign = numChannels * (bitsPerSample / 8);
    header.bitsPerSample = bitsPerSample;

    // Fill data chunk
    header.data[0] = 'd'; header.data[1] = 'a'; header.data[2] = 't'; header.data[3] = 'a';
    header.dataSize = dataSizeBytes;

    fwrite(&header, sizeof(WavHeader), 1, file);

    // Convert float to int16 and write
    int16_t* pcmData = (int16_t*)malloc(dataSizeBytes);
    if (!pcmData) {
        fclose(file);
        return -1;
    }

    for (size_t i = 0; i < totalSamples; i++) {
        float sample = audioData[i];
        // Clip to [-1.0, 1.0]
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;

        // Convert to 16-bit PCM
        pcmData[i] = (int16_t)(sample * 32767.0f);
    }

    fwrite(pcmData, sizeof(int16_t), totalSamples, file);

    free(pcmData);
    fclose(file);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "core/dimension_chorus.h"
#include "core/dsp_common.h"
#include "test_signals.h"
#include "wav_writer.h"

#define DURATION_SECONDS 5
#define TOTAL_FRAMES ((int)(DSP_SAMPLE_RATE * DURATION_SECONDS))

void write_mono_as_stereo(const char* filename, const float* monoBuffer, size_t numFrames) {
    float* stereoBuffer = (float*)malloc(numFrames * 2 * sizeof(float));
    for (size_t i = 0; i < numFrames; i++) {
        stereoBuffer[i*2] = monoBuffer[i];
        stereoBuffer[i*2+1] = monoBuffer[i];
    }
    write_wav_file(filename, stereoBuffer, numFrames, (uint32_t)DSP_SAMPLE_RATE);
    free(stereoBuffer);
}

void render_test(int mode, const char* signal_type, const float* inMono) {
    printf("Rendering Mode %d with signal: %s...\n", mode + 1, signal_type);

    DimensionChorusState chorus;
    DimensionChorus_Init(&chorus);
    DimensionChorus_SetMode(&chorus, (DimensionMode)mode);

    float* outStereo = (float*)calloc(TOTAL_FRAMES * 2, sizeof(float));
    float* outMonoSum = (float*)calloc(TOTAL_FRAMES, sizeof(float));

    size_t frames_processed = 0;
    while (frames_processed < TOTAL_FRAMES) {
        size_t block_frames = DSP_BLOCK_SIZE;
        if (frames_processed + DSP_BLOCK_SIZE > TOTAL_FRAMES) {
            block_frames = TOTAL_FRAMES - frames_processed;
        }

        DimensionChorus_ProcessBlock_Inspect(&chorus,
                                             &inMono[frames_processed],
                                             &outStereo[frames_processed * 2],
                                             NULL, NULL, NULL,
                                             &outMonoSum[frames_processed],
                                             block_frames);

        frames_processed += block_frames;
    }

    char filename[256];
    snprintf(filename, sizeof(filename), "tests/output/%s_mode_%d.wav", signal_type, mode + 1);
    write_wav_file(filename, outStereo, TOTAL_FRAMES, (uint32_t)DSP_SAMPLE_RATE);

    snprintf(filename, sizeof(filename), "tests/output/%s_mode_%d_monosum.wav", signal_type, mode + 1);
    write_mono_as_stereo(filename, outMonoSum, TOTAL_FRAMES);

    free(outStereo);
    free(outMonoSum);
}

void print_help() {
    printf("Dimension Chorus Offline Runner\n");
    printf("Usage:\n");
    printf("  ./offline_runner [options]\n\n");
    printf("Options:\n");
    printf("  --mode=<1|2|3|4>    Process a specific mode.\n");
    printf("  --signal=<type>     Type of signal: sine, sweep, impulse, noise, chord.\n");
    printf("  --batch             Render all modes with all signals.\n");
}

int main(int argc, char** argv) {
    int target_mode = -1;
    char target_signal[64] = "sweep";
    int batch_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--mode=", 7) == 0) {
            target_mode = atoi(argv[i] + 7) - 1;
        } else if (strncmp(argv[i], "--signal=", 9) == 0) {
            strncpy(target_signal, argv[i] + 9, sizeof(target_signal) - 1);
        } else if (strcmp(argv[i], "--batch") == 0) {
            batch_mode = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
    }

    // We expect the bash script or user to ensure output folder exists
    // Let's create it via system call just in case
    system("mkdir -p tests/output");

    float* inMono = (float*)calloc(TOTAL_FRAMES, sizeof(float));
    if (!inMono) {
        printf("Failed to allocate input buffer.\n");
        return 1;
    }

    if (batch_mode) {
        const char* signals[] = {"sine", "sweep", "impulse", "noise", "chord"};
        for (int s = 0; s < 5; s++) {
            memset(inMono, 0, TOTAL_FRAMES * sizeof(float));
            if (strcmp(signals[s], "sine") == 0) generate_sine(inMono, TOTAL_FRAMES, 440.0f, DSP_SAMPLE_RATE);
            else if (strcmp(signals[s], "sweep") == 0) generate_sweep(inMono, TOTAL_FRAMES, DSP_SAMPLE_RATE, DURATION_SECONDS);
            else if (strcmp(signals[s], "impulse") == 0) generate_impulse(inMono, TOTAL_FRAMES);
            else if (strcmp(signals[s], "noise") == 0) generate_noise(inMono, TOTAL_FRAMES);
            else if (strcmp(signals[s], "chord") == 0) generate_chord(inMono, TOTAL_FRAMES, DSP_SAMPLE_RATE);

            // Save dry ref
            char dry_file[256];
            snprintf(dry_file, sizeof(dry_file), "tests/output/%s_dry.wav", signals[s]);
            write_mono_as_stereo(dry_file, inMono, TOTAL_FRAMES);

            for (int m = 0; m < 4; m++) {
                render_test(m, signals[s], inMono);
            }
        }
    } else {
        // Single mode run (defaults to mode 1 if not provided)
        if (target_mode < 0 || target_mode > 3) target_mode = 0;

        if (strcmp(target_signal, "sine") == 0) generate_sine(inMono, TOTAL_FRAMES, 440.0f, DSP_SAMPLE_RATE);
        else if (strcmp(target_signal, "sweep") == 0) generate_sweep(inMono, TOTAL_FRAMES, DSP_SAMPLE_RATE, DURATION_SECONDS);
        else if (strcmp(target_signal, "impulse") == 0) generate_impulse(inMono, TOTAL_FRAMES);
        else if (strcmp(target_signal, "noise") == 0) generate_noise(inMono, TOTAL_FRAMES);
        else if (strcmp(target_signal, "chord") == 0) generate_chord(inMono, TOTAL_FRAMES, DSP_SAMPLE_RATE);
        else generate_sweep(inMono, TOTAL_FRAMES, DSP_SAMPLE_RATE, DURATION_SECONDS); // fallback

        char dry_file[256];
        snprintf(dry_file, sizeof(dry_file), "tests/output/%s_dry.wav", target_signal);
        write_mono_as_stereo(dry_file, inMono, TOTAL_FRAMES);

        render_test(target_mode, target_signal, inMono);
    }

    free(inMono);
    printf("Offline Runner Completed.\n");
    return 0;
}

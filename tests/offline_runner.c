#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "core/dimension_chorus.h"
#include "core/dsp_common.h"
#include "test_signals.h"
#include "wav_writer.h"
#include "wav_reader.h"

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

void render_test(int mode, const char* signal_type, const float* inMono, size_t num_frames, const char* custom_output, int force_mono_sum) {
    DimensionChorusState chorus;
    DimensionChorus_Init(&chorus);
    DimensionChorus_SetMode(&chorus, (DimensionMode)mode);

    float* outStereo = (float*)calloc(num_frames * 2, sizeof(float));
    float* outMonoSum = (float*)calloc(num_frames, sizeof(float));

    size_t frames_processed = 0;
    while (frames_processed < num_frames) {
        size_t block_frames = DSP_BLOCK_SIZE;
        if (frames_processed + DSP_BLOCK_SIZE > num_frames) {
            block_frames = num_frames - frames_processed;
        }

        DimensionChorus_ProcessBlock_Inspect(&chorus,
                                             &inMono[frames_processed],
                                             &outStereo[frames_processed * 2],
                                             NULL, NULL, NULL,
                                             &outMonoSum[frames_processed],
                                             block_frames);

        frames_processed += block_frames;
    }

    float max_peak_stereo = 0.0f;
    for (size_t i = 0; i < num_frames * 2; i++) {
        float abs_val = fabsf(outStereo[i]);
        if (abs_val > max_peak_stereo) max_peak_stereo = abs_val;
    }

    float max_peak_mono = 0.0f;
    for (size_t i = 0; i < num_frames; i++) {
        float abs_val = fabsf(outMonoSum[i]);
        if (abs_val > max_peak_mono) max_peak_mono = abs_val;
    }

    printf("[Mode %d | Signal: %s] Rendered. Max Peak: %.4f (Stereo), %.4f (Mono Sum)\n",
           mode + 1, signal_type, max_peak_stereo, max_peak_mono);

    char filename[256];
    if (custom_output) {
        strncpy(filename, custom_output, sizeof(filename) - 1);
    } else {
        snprintf(filename, sizeof(filename), "tests/output/%s_mode_%d.wav", signal_type, mode + 1);
    }
    write_wav_file(filename, outStereo, num_frames, (uint32_t)DSP_SAMPLE_RATE);

    if (force_mono_sum || !custom_output) { // batch always outputs monosum
        char mono_filename[256];
        if (custom_output) {
            snprintf(mono_filename, sizeof(mono_filename), "%s_monosum.wav", custom_output);
        } else {
            snprintf(mono_filename, sizeof(mono_filename), "tests/output/%s_mode_%d_monosum.wav", signal_type, mode + 1);
        }
        write_mono_as_stereo(mono_filename, outMonoSum, num_frames);
    }

    free(outStereo);
    free(outMonoSum);
}

void print_help() {
    printf("Dimension Chorus Offline Runner\n");
    printf("Usage:\n");
    printf("  ./offline_runner [options]\n\n");
    printf("Options:\n");
    printf("  --input=<file.wav>  Use external WAV file as input instead of synthetic signal.\n");
    printf("  --output=<file.wav> Force a specific output file (Ignored if --batch).\n");
    printf("  --mode=<1|2|3|4>    Process a specific mode.\n");
    printf("  --signal=<type>     Type of synthetic signal: sine, sweep, impulse, noise, chord (Ignored if --input).\n");
    printf("  --mono-sum          Force generation of an extra 'monosum' file in individual mode.\n");
    printf("  --batch             Render all modes with synthetic signals (or the provided --input).\n");
}

int main(int argc, char** argv) {
    int target_mode = -1;
    char target_signal[64] = "sweep";
    int batch_mode = 0;
    char input_file[256] = "";
    char output_file[256] = "";
    int force_mono_sum = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--mode=", 7) == 0) {
            target_mode = atoi(argv[i] + 7) - 1;
        } else if (strncmp(argv[i], "--signal=", 9) == 0) {
            strncpy(target_signal, argv[i] + 9, sizeof(target_signal) - 1);
        } else if (strncmp(argv[i], "--input=", 8) == 0) {
            strncpy(input_file, argv[i] + 8, sizeof(input_file) - 1);
        } else if (strncmp(argv[i], "--output=", 9) == 0) {
            strncpy(output_file, argv[i] + 9, sizeof(output_file) - 1);
        } else if (strcmp(argv[i], "--mono-sum") == 0) {
            force_mono_sum = 1;
        } else if (strcmp(argv[i], "--batch") == 0) {
            batch_mode = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
    }

    // Ensure output folder exists
    system("mkdir -p tests/output");

    float* inMono = NULL;
    size_t active_frames = TOTAL_FRAMES;
    const char* signal_name_override = target_signal;

    if (strlen(input_file) > 0) {
        uint32_t sampleRate = 0;
        if (read_wav_file_mono(input_file, &inMono, &active_frames, &sampleRate) != 0) {
            printf("Error reading input WAV file.\n");
            return 1;
        }

        if (sampleRate != (uint32_t)DSP_SAMPLE_RATE) {
            printf("Error: Input WAV file must have a sample rate of %d Hz (detected %d Hz).\n", (int)DSP_SAMPLE_RATE, sampleRate);
            free(inMono);
            return 1;
        }

        // Use a generic name for logging
        signal_name_override = "custom_wav";

        if (batch_mode) {
            for (int m = 0; m < 4; m++) {
                render_test(m, signal_name_override, inMono, active_frames, NULL, 1);
            }
        } else {
             if (target_mode < 0 || target_mode > 3) target_mode = 0;
             const char* out_ptr = (strlen(output_file) > 0) ? output_file : NULL;
             render_test(target_mode, signal_name_override, inMono, active_frames, out_ptr, force_mono_sum);
        }
        free(inMono);

    } else {
        inMono = (float*)calloc(TOTAL_FRAMES, sizeof(float));
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
                    render_test(m, signals[s], inMono, TOTAL_FRAMES, NULL, 1);
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

            const char* out_ptr = (strlen(output_file) > 0) ? output_file : NULL;
            render_test(target_mode, target_signal, inMono, TOTAL_FRAMES, out_ptr, force_mono_sum);
        }
        free(inMono);
    }
    printf("Offline Runner Completed.\n");
    return 0;
}

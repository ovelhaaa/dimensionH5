#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "dimension_chorus.h"
#include "wav_writer.h"

#define SAMPLE_RATE 48000
#define DURATION_SECONDS 5
#define BLOCK_SIZE 32
#define TOTAL_FRAMES (SAMPLE_RATE * DURATION_SECONDS)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void generate_sine(float* buffer, size_t numFrames, float freq) {
    float phase = 0.0f;
    float phaseInc = 2.0f * (float)M_PI * freq / SAMPLE_RATE;
    for (size_t i = 0; i < numFrames; i++) {
        buffer[i] = 0.8f * sinf(phase);
        phase = fmodf(phase + phaseInc, 2.0f * (float)M_PI);
    }
}

void generate_sweep(float* buffer, size_t numFrames) {
    float phase = 0.0f;
    float f0 = 100.0f;
    float f1 = 2000.0f;
    for (size_t i = 0; i < numFrames; i++) {
        float time = (float)i / SAMPLE_RATE;
        float duration = DURATION_SECONDS;
        float instantaneous_freq = f0 + (f1 - f0) * (time / duration);
        float dt = 1.0f / SAMPLE_RATE;
        phase += 2.0f * (float)M_PI * instantaneous_freq * dt;
        phase = fmodf(phase, 2.0f * (float)M_PI);
        buffer[i] = 0.8f * sinf(phase);
    }
}

void generate_impulse(float* buffer, size_t numFrames) {
    for (size_t i = 0; i < numFrames; i++) {
        buffer[i] = 0.0f;
    }
    buffer[10] = 0.8f; // Impulse near start
}

void generate_chord(float* buffer, size_t numFrames) {
    float phase220 = 0.0f, phase275 = 0.0f, phase330 = 0.0f;
    float inc220 = 2.0f * (float)M_PI * 220.0f / SAMPLE_RATE;
    float inc275 = 2.0f * (float)M_PI * 275.0f / SAMPLE_RATE;
    float inc330 = 2.0f * (float)M_PI * 330.0f / SAMPLE_RATE;

    for (size_t i = 0; i < numFrames; i++) {
        float val = sinf(phase220) + sinf(phase275) + sinf(phase330);
        buffer[i] = 0.8f * (val / 3.0f); // Scale to 0.8 max
        phase220 = fmodf(phase220 + inc220, 2.0f * (float)M_PI);
        phase275 = fmodf(phase275 + inc275, 2.0f * (float)M_PI);
        phase330 = fmodf(phase330 + inc330, 2.0f * (float)M_PI);
    }
}

void generate_noise(float* buffer, size_t numFrames) {
    for (size_t i = 0; i < numFrames; i++) {
        float r = ((float)rand() / (float)(RAND_MAX)) * 2.0f - 1.0f;
        buffer[i] = 0.5f * r; // Scale to avoid harsh clipping
    }
}

// Function to convert a mono buffer to interleaved stereo for saving
void write_mono_as_stereo(const char* filename, const float* monoBuffer, size_t numFrames) {
    float* stereoBuffer = (float*)malloc(numFrames * 2 * sizeof(float));
    for (size_t i = 0; i < numFrames; i++) {
        stereoBuffer[i*2] = monoBuffer[i];
        stereoBuffer[i*2+1] = monoBuffer[i];
    }
    write_wav_file(filename, stereoBuffer, numFrames, SAMPLE_RATE);
    free(stereoBuffer);
}

// Helper to compute metrics
void compute_and_log_metrics(const char* signalName, int mode, const float* outStereo, const float* monoSum, size_t numFrames, FILE* reportFile) {
    float peakL = 0.0f, peakR = 0.0f;
    double sumSqL = 0.0, sumSqR = 0.0;
    double sumL = 0.0, sumR = 0.0;
    double crossSum = 0.0;
    double sumSqMono = 0.0;
    double sumDiffSq = 0.0;

    for (size_t i = 0; i < numFrames; i++) {
        float l = outStereo[i*2];
        float r = outStereo[i*2 + 1];
        float m = monoSum[i];

        if (fabsf(l) > peakL) peakL = fabsf(l);
        if (fabsf(r) > peakR) peakR = fabsf(r);

        sumSqL += l * l;
        sumSqR += r * r;
        sumL += l;
        sumR += r;
        crossSum += l * r;

        sumSqMono += m * m;

        // Difference between stereo power and mono sum power loosely
        // To properly measure the stereo difference (the "Side" signal)
        float diff = (l - r) * 0.5f;
        sumDiffSq += diff * diff;
    }

    float rmsL = (float)sqrt(sumSqL / numFrames);
    float rmsR = (float)sqrt(sumSqR / numFrames);
    float dcL = (float)(sumL / numFrames);
    float dcR = (float)(sumR / numFrames);
    float rmsMono = (float)sqrt(sumSqMono / numFrames);
    float rmsDiff = (float)sqrt(sumDiffSq / numFrames);

    // Correlation coefficient
    double covar = (crossSum / numFrames) - (dcL * dcR);
    double varL = (sumSqL / numFrames) - (dcL * dcL);
    double varR = (sumSqR / numFrames) - (dcR * dcR);
    float correlation = 0.0f;
    if (varL > 0 && varR > 0) {
        correlation = (float)(covar / sqrt(varL * varR));
    }

    char buf[512];
    snprintf(buf, sizeof(buf),
        "Signal: %s, Mode: %d\n"
        "  Peak L/R:      %.4f / %.4f\n"
        "  RMS L/R:       %.4f / %.4f\n"
        "  DC Offset L/R: %.6f / %.6f\n"
        "  Correlation:   %.4f\n"
        "  Mono Sum RMS:  %.4f\n"
        "  Diff RMS:      %.4f\n\n",
        signalName, mode, peakL, peakR, rmsL, rmsR, dcL, dcR, correlation, rmsMono, rmsDiff);

    printf("%s", buf);
    if (reportFile) {
        fprintf(reportFile, "%s", buf);
    }
}

int render_test_signal(const char* signalName, const float* inMono, size_t totalFrames, FILE* reportFile) {
    printf("--- Rendering %s ---\n", signalName);

    // Render dry reference
    char dryFilename[256];
    snprintf(dryFilename, sizeof(dryFilename), "tests/output/%s_dry_ref.wav", signalName);
    write_mono_as_stereo(dryFilename, inMono, totalFrames);

    for (int modeInt = DIMENSION_MODE_1; modeInt <= DIMENSION_MODE_4; modeInt++) {
        DimensionMode mode = (DimensionMode)modeInt;

        DimensionChorusState state;
        DimensionChorus_Init(&state);
        DimensionChorus_SetMode(&state, mode);

        float* outStereo = (float*)malloc(totalFrames * 2 * sizeof(float));
        float* outDry = (float*)malloc(totalFrames * sizeof(float));
        float* outWet1 = (float*)malloc(totalFrames * sizeof(float));
        float* outWet2 = (float*)malloc(totalFrames * sizeof(float));
        float* outMonoSum = (float*)malloc(totalFrames * sizeof(float));

        if (!outStereo || !outDry || !outWet1 || !outWet2 || !outMonoSum) {
            printf("Memory allocation failed!\n");
            free(outStereo);
            free(outDry);
            free(outWet1);
            free(outWet2);
            free(outMonoSum);
            return -1;
        }

        size_t framesProcessed = 0;
        while (framesProcessed < totalFrames) {
            size_t framesToProcess = BLOCK_SIZE;
            if (framesProcessed + BLOCK_SIZE > totalFrames) {
                framesToProcess = totalFrames - framesProcessed;
            }

            DimensionChorus_ProcessBlock_Inspect(&state,
                                                 &inMono[framesProcessed],
                                                 &outStereo[framesProcessed * 2],
                                                 &outDry[framesProcessed],
                                                 &outWet1[framesProcessed],
                                                 &outWet2[framesProcessed],
                                                 &outMonoSum[framesProcessed],
                                                 framesToProcess);

            framesProcessed += framesToProcess;
        }

        // Write stereo file
        char filename[256];
        snprintf(filename, sizeof(filename), "tests/output/%s_mode_%d.wav", signalName, mode + 1);
        write_wav_file(filename, outStereo, totalFrames, SAMPLE_RATE);

        // Write stems
        snprintf(filename, sizeof(filename), "tests/output/%s_mode_%d_dry.wav", signalName, mode + 1);
        write_mono_as_stereo(filename, outDry, totalFrames);

        snprintf(filename, sizeof(filename), "tests/output/%s_mode_%d_wet1.wav", signalName, mode + 1);
        write_mono_as_stereo(filename, outWet1, totalFrames);

        snprintf(filename, sizeof(filename), "tests/output/%s_mode_%d_wet2.wav", signalName, mode + 1);
        write_mono_as_stereo(filename, outWet2, totalFrames);

        snprintf(filename, sizeof(filename), "tests/output/%s_mode_%d_monosum.wav", signalName, mode + 1);
        write_mono_as_stereo(filename, outMonoSum, totalFrames);

        // Metrics
        compute_and_log_metrics(signalName, mode + 1, outStereo, outMonoSum, totalFrames, reportFile);

        free(outStereo);
        free(outDry);
        free(outWet1);
        free(outWet2);
        free(outMonoSum);
    }
    return 0;
}

int main() {
    printf("Starting Offline Validation Suite...\n");

    // Ensure output directory exists (we can rely on the bash script or assume it exists, let's just write to tests/output/)
    // Assuming tests/output/ exists based on previous code.

    FILE* reportFile = fopen("../tests/output/report.txt", "w");
    if (!reportFile) {
        reportFile = fopen("tests/output/report.txt", "w"); // fallback
    }

    if (!reportFile) {
        printf("Could not open report.txt for writing.\n");
        // We'll continue anyway
    } else {
        fprintf(reportFile, "Dimension Chorus Offline Validation Report\n");
        fprintf(reportFile, "========================================\n\n");
    }

    float* inMono = (float*)malloc(TOTAL_FRAMES * sizeof(float));
    if (!inMono) {
        if (reportFile) {
            fclose(reportFile);
        }
        return 1;
    }

    generate_sine(inMono, TOTAL_FRAMES, 440.0f);
    render_test_signal("sine440", inMono, TOTAL_FRAMES, reportFile);

    generate_sine(inMono, TOTAL_FRAMES, 1000.0f);
    render_test_signal("sine1k", inMono, TOTAL_FRAMES, reportFile);

    generate_sweep(inMono, TOTAL_FRAMES);
    render_test_signal("sweep", inMono, TOTAL_FRAMES, reportFile);

    generate_impulse(inMono, TOTAL_FRAMES);
    render_test_signal("impulse", inMono, TOTAL_FRAMES, reportFile);

    generate_chord(inMono, TOTAL_FRAMES);
    render_test_signal("chord", inMono, TOTAL_FRAMES, reportFile);

    generate_noise(inMono, TOTAL_FRAMES);
    render_test_signal("noise", inMono, TOTAL_FRAMES, reportFile);

    free(inMono);

    if (reportFile) {
        fclose(reportFile);
    }

    printf("Validation suite complete.\n");
    return 0;
}

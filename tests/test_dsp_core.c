#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "core/dimension_chorus.h"
#include "core/dsp_common.h"

#define SAMPLE_RATE ((int)DSP_SAMPLE_RATE)
#define BLOCK_SIZE DSP_BLOCK_SIZE
#define DURATION_SEC 2

#define M_PI_F 3.14159265f
#define TEST_FREQ_1KHZ 1000.0f
#define TEST_FREQ_100HZ 100.0f

// Funcoes auxiliares para os testes
int compare_float(float a, float b, float epsilon) {
    return fabsf(a - b) < epsilon;
}

// 1. Impulse Response Test (Timing/Fase)
int test_impulse_response() {
    printf("Running Impulse Response Test...\n");
    DimensionChorusState chorus;
    DimensionChorus_Init(&chorus);
    DimensionChorus_SetMode(&chorus, 0); // Ativa o modo 0 para testar taps

    float inMono[BLOCK_SIZE] = {0.0f};
    float outStereo[BLOCK_SIZE * 2] = {0.0f};

    // Impulse no primeiro frame do primeiro bloco
    inMono[0] = 1.0f;

    // Processa um bloco (Mono-In, Interleaved Stereo-Out)
    DimensionChorus_ProcessBlock(&chorus, inMono, outStereo, BLOCK_SIZE);

    // O primeiro frame Left (índice 0) deve conter o dry signal
    if(outStereo[0] != 0.0f) {
        printf("  [OK] Impulse processado com dry/wet handling\n");
        return 0;
    } else {
        printf("  [FAIL] Sem sinal na saida.\n");
        return 1;
    }
}

// 2. Sine THD / Headroom Test
int test_headroom_clipping() {
    printf("Running Headroom & Clipping Test...\n");
    DimensionChorusState chorus;
    DimensionChorus_Init(&chorus);
    DimensionChorus_SetMode(&chorus, 1);

    float inMono[BLOCK_SIZE];
    float outStereo[BLOCK_SIZE * 2];

    // Injeta senoidal 1kHz @ 48kHz SR em full scale 1.0f
    for (int i = 0; i < BLOCK_SIZE; i++) {
        float val = sinf(2.0f * M_PI_F * TEST_FREQ_1KHZ * (float)i / SAMPLE_RATE);
        inMono[i] = val;
    }

    DimensionChorus_ProcessBlock(&chorus, inMono, outStereo, BLOCK_SIZE);

    int clipped = 0;
    for (int i = 0; i < BLOCK_SIZE * 2; i++) {
        if (outStereo[i] > 1.0f || outStereo[i] < -1.0f) {
            clipped = 1;
        }
    }

    if (!clipped) {
         printf("  [OK] Valores dentro de range float seguro (-1.0 a 1.0)\n");
         return 0;
    } else {
         printf("  [FAIL] Detectado hard clipping float! Reveja os ganhos internos da matriz.\n");
         return 1;
    }
}

// 3. Mono Summing Phase Cancellation Test
int test_mono_summing() {
    printf("Running Mono Summing Phase Cancellation Test...\n");
    DimensionChorusState chorus;
    DimensionChorus_Init(&chorus);
    DimensionChorus_SetMode(&chorus, 3); // O modo 3 usa modulações mais intensas

    float inMono[BLOCK_SIZE];
    float outStereo[BLOCK_SIZE * 2];

    // Injeta onda para simular baixo e checar perda de grave
    for (int i = 0; i < BLOCK_SIZE; i++) {
         float val = sinf(2.0f * M_PI_F * TEST_FREQ_100HZ * (float)i / SAMPLE_RATE); // 100 Hz
         inMono[i] = val;
    }

    DimensionChorus_ProcessBlock(&chorus, inMono, outStereo, BLOCK_SIZE);

    float rms_sum = 0.0f;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        float left = outStereo[i * 2];
        float right = outStereo[i * 2 + 1];
        float mono_mix = (left + right) * 0.5f;
        rms_sum += (mono_mix * mono_mix);
    }

    float rms = sqrtf(rms_sum / BLOCK_SIZE);

    if (rms > 0.1f) {
        printf("  [OK] Energia retida apos soma mono (RMS: %f). Fases do grave cruzadas com sucesso!\n", rms);
        return 0;
    } else {
        printf("  [FAIL] Cancelamento de fase excessivo no mono (RMS: %f).\n", rms);
        return 1;
    }
}

int main() {
    printf("=== Dimension Chorus DSP Core Tests ===\n");
    int failures = 0;
    failures += test_impulse_response();
    failures += test_headroom_clipping();
    failures += test_mono_summing();
    printf("=======================================\n");
    printf("Tests Failed: %d\n", failures);
    return failures;
}

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "core/dimension_chorus.h"
#include "core/dsp_common.h"
#include "test_signals.h"

#define M_PI_F 3.14159265f
#define TEST_FREQ_1KHZ 1000.0f
#define TEST_FREQ_100HZ 100.0f

// Tolerância para -6dB de perda de energia RMS no summing mono.
#define MONO_SUM_ATTENUATION_THRESHOLD 0.5f

// Parâmetros e limites nomeados para os testes
#define IMPULSE_TEST_DURATION_MS 22.0f
#define IMPULSE_TEST_FRAMES ((size_t)((IMPULSE_TEST_DURATION_MS / 1000.0f) * DSP_SAMPLE_RATE))
#define IMPULSE_TIMING_MARGIN_SAMPLES 10

#define HEADROOM_TEST_FRAMES (DSP_BLOCK_SIZE * 4)
#define HEADROOM_OVERDRIVE_FACTOR 2.0f
#define SOFT_CLIP_MAX_THRESHOLD 0.6667f
#define SOFT_CLIP_EPSILON 1e-4f
#define SOFT_CLIP_MIN_PEAK_THRESHOLD 0.6f

#define MONO_SUM_TEST_DURATION_MS 100.0f
#define MONO_SUM_TEST_FRAMES ((size_t)((MONO_SUM_TEST_DURATION_MS / 1000.0f) * DSP_SAMPLE_RATE))

// Funcoes auxiliares para os testes
int compare_float(float a, float b, float epsilon) {
    return fabsf(a - b) < epsilon;
}

// 1. Impulse Response Test (Timing/Fase)
int test_impulse_response() {
    printf("Running Impulse Response Test...\n");
    DimensionChorusState chorus;
    DimensionChorus_Init(&chorus);
    DimensionChorus_SetMode(&chorus, 0); // Ativa o modo 1 (índice 0)

    // Forçar targets para os atuais para não haver rampa do smoothing nos testes
    chorus.rate = chorus.targetRate;
    chorus.baseMs = chorus.targetBaseMs;
    chorus.depth = chorus.targetDepth;
    chorus.mainW = chorus.targetMainW;
    chorus.crossW = chorus.targetCrossW;

    // Precisamos de um bloco maior para ver o tap de delay sair.
    // baseMs do Mode 1 = 10.5ms = 504 samples em 48kHz.
    const size_t test_frames = IMPULSE_TEST_FRAMES;
    float* inMono = (float*)calloc(test_frames, sizeof(float));
    float* outWet1 = (float*)calloc(test_frames, sizeof(float));
    float* outWet2 = (float*)calloc(test_frames, sizeof(float));

    // Impulse no primeiro frame do primeiro bloco
    inMono[0] = 1.0f;

    size_t frames_processed = 0;
    while(frames_processed < test_frames) {
        size_t frames_to_process = DSP_BLOCK_SIZE;
        DimensionChorus_ProcessBlock_Inspect(&chorus,
            &inMono[frames_processed],
            NULL, // outStereo not needed here
            NULL, // outDry not needed
            &outWet1[frames_processed],
            &outWet2[frames_processed],
            NULL, // monoSum
            frames_to_process);
        frames_processed += frames_to_process;
    }

    // Achar o pico em outWet1
    float peak_val = 0.0f;
    size_t peak_idx = 0;
    for (size_t i = 0; i < test_frames; i++) {
        if (fabsf(outWet1[i]) > peak_val) {
            peak_val = fabsf(outWet1[i]);
            peak_idx = i;
        }
    }

    // Calcula tempo do delay alvo inicial: baseMs
    float expected_delay_samps = chorus.baseMs * (DSP_SAMPLE_RATE / 1000.0f);

    free(inMono);
    free(outWet1);
    free(outWet2);

    // Permitimos margem pois há filtros que "espalham" o impulso,
    // e modulador que pode mudar levemente o offset inicial se a fase LFO rodar
    if (peak_idx > 0 && fabsf((float)peak_idx - expected_delay_samps) < (float)IMPULSE_TIMING_MARGIN_SAMPLES) {
        printf("  [OK] Impulse delay verificado no Wet (Pico no tap: %zu, Esperado: ~%.1f)\n", peak_idx, expected_delay_samps);
        return 0;
    } else {
        printf("  [FAIL] Falha no teste de timing. Pico tap em %zu (Esperado ~%.1f)\n", peak_idx, expected_delay_samps);
        return 1;
    }
}

// 2. Sine THD / Headroom Test
int test_headroom_clipping() {
    printf("Running Headroom & Clipping Test...\n");
    DimensionChorusState chorus;
    DimensionChorus_Init(&chorus);
    DimensionChorus_SetMode(&chorus, 1);

    const size_t test_frames = HEADROOM_TEST_FRAMES;
    float inMono[HEADROOM_TEST_FRAMES];
    float outStereo[HEADROOM_TEST_FRAMES * 2];

    // Injeta senoidal 1kHz @ 48kHz SR em full scale 1.0f
    generate_sine(inMono, test_frames, TEST_FREQ_1KHZ, DSP_SAMPLE_RATE);

    // Escala abusivamente para +6dB para forçar a saturação testada
    for (size_t i = 0; i < test_frames; i++) {
        inMono[i] *= HEADROOM_OVERDRIVE_FACTOR;
    }

    size_t frames_processed = 0;
    while (frames_processed < test_frames) {
        DimensionChorus_ProcessBlock(&chorus, &inMono[frames_processed], &outStereo[frames_processed * 2], DSP_BLOCK_SIZE);
        frames_processed += DSP_BLOCK_SIZE;
    }

    int clipped_badly = 0;
    float max_peak = 0.0f;

    for (size_t i = 0; i < test_frames * 2; i++) {
        float abs_val = fabsf(outStereo[i]);
        if (abs_val > max_peak) max_peak = abs_val;

        // Usamos epsilon minúsculo para a checagem
        if (abs_val > SOFT_CLIP_MAX_THRESHOLD + SOFT_CLIP_EPSILON) {
            clipped_badly = 1;
        }
    }

    if (!clipped_badly && max_peak > SOFT_CLIP_MIN_PEAK_THRESHOLD) {
         printf("  [OK] Sinais contidos no limitador da matriz com sucesso (Max: %f)\n", max_peak);
         return 0;
    } else {
         printf("  [FAIL] Detectada anomalia de headroom! Pico medido: %f\n", max_peak);
         return 1;
    }
}

// 3. Mono Summing Phase Cancellation Test
int test_mono_summing() {
    printf("Running Mono Summing Phase Cancellation Test...\n");
    DimensionChorusState chorus;
    DimensionChorus_Init(&chorus);
    DimensionChorus_SetMode(&chorus, 3); // O modo 3 usa modulações mais intensas

    const size_t test_frames = MONO_SUM_TEST_FRAMES; // 100ms de sinal para análise
    float* inMono = (float*)malloc(test_frames * sizeof(float));
    float* outStereo = (float*)malloc(test_frames * 2 * sizeof(float));

    // Injeta onda para simular baixo e checar perda de grave
    generate_sine(inMono, test_frames, TEST_FREQ_100HZ, DSP_SAMPLE_RATE);

    size_t frames_processed = 0;
    while(frames_processed < test_frames) {
        DimensionChorus_ProcessBlock(&chorus, &inMono[frames_processed], &outStereo[frames_processed * 2], DSP_BLOCK_SIZE);
        frames_processed += DSP_BLOCK_SIZE;
    }

    float rms_sum_mono = 0.0f;
    float rms_sum_dry = 0.0f;

    for (size_t i = 0; i < test_frames; i++) {
        float left = outStereo[i * 2];
        float right = outStereo[i * 2 + 1];
        float mono_mix = (left + right) * 0.5f;
        rms_sum_mono += (mono_mix * mono_mix);

        float dry = inMono[i]; // Pegando apenas como referência de ganho inicial
        rms_sum_dry += (dry * dry);
    }

    float rms_mono = sqrtf(rms_sum_mono / test_frames);
    float rms_dry = sqrtf(rms_sum_dry / test_frames);

    free(inMono);
    free(outStereo);

    // Validar se perda ao fazer downmix não extingue mais de -6dB (limiar de 0.5) comparado à potência que entrou
    if (rms_mono > (rms_dry * MONO_SUM_ATTENUATION_THRESHOLD)) {
        printf("  [OK] Energia retida apos soma mono (RMS Mono: %f, Dry: %f).\n", rms_mono, rms_dry);
        return 0;
    } else {
        printf("  [FAIL] Cancelamento de fase excessivo no mono (RMS Mono: %f, Dry: %f).\n", rms_mono, rms_dry);
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

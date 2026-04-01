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
#define SOFT_CLIP_MAX_THRESHOLD 0.99f
#define SOFT_CLIP_EPSILON 1e-4f
#define SOFT_CLIP_MIN_PEAK_THRESHOLD 0.6f
#define CENTER_PRESERVE_MIN 0.70f
#define STEREO_DIFF_MIN 0.01f

#define MONO_SUM_TEST_DURATION_MS 100.0f
#define MONO_SUM_TEST_FRAMES ((size_t)((MONO_SUM_TEST_DURATION_MS / 1000.0f) * DSP_SAMPLE_RATE))

// Funcoes auxiliares para os testes
int compare_float(float a, float b, float epsilon) {
    return fabsf(a - b) < epsilon;
}

/**
 * Verify that the compiled mode parameter table matches expected rate, base, and depth values.
 *
 * Compares each mode's `rateHz`, `baseMs`, and `depthMs` against hardcoded expected values
 * using a tolerance of 1e-6; prints a pass or fail message for the test.
 *
 * @returns 0 on success, 1 if any mode's parameters differ from the expected values.
 */
int test_mode_table_ranges() {
    printf("Running Mode Table Range Test...\n");
    const float expectedRates[DIMENSION_MODE_COUNT] = {0.16f, 0.23f, 0.34f, 0.48f};
    const float expectedBaseMs[DIMENSION_MODE_COUNT] = {9.9f, 10.4f, 10.9f, 11.6f};
    const float expectedDepthMs[DIMENSION_MODE_COUNT] = {0.42f, 0.58f, 0.78f, 0.98f};

    for (int i = 0; i < DIMENSION_MODE_COUNT; ++i) {
        const DimensionModeParams p = DimensionMode_GetParams((DimensionMode)i);
        if (!compare_float(p.rateHz, expectedRates[i], 1e-6f) ||
            !compare_float(p.baseMs, expectedBaseMs[i], 1e-6f) ||
            !compare_float(p.depthMs, expectedDepthMs[i], 1e-6f)) {
            printf("  [FAIL] Mode %d mismatch (rate/base/depth).\n", i + 1);
            return 1;
        }
    }

    printf("  [OK] Mode table sweet spots loaded as expected.\n");
    return 0;
}

/**
 * Validate resolution logic for combined Dimension modes.
 *
 * Performs three checks: (1) a single-bit combo mask yields identical parameters to the corresponding single mode,
 * (2) a two-mode combo yields a rate equal to the maximum of the selected modes and a depth equal to 0.7 times the sum
 * of their depths, and (3) a full-mask combo clamps depth and main wet to expected upper bounds.
 *
 * @returns 0 on success, 1 if any check fails.
 */
int test_combo_mode_logic() {
    printf("Running Combo Mode Logic Test...\n");

    // 1. Single selection in combo mask should equal single mode for all modes
    for (int i = 0; i < DIMENSION_MODE_COUNT; i++) {
        DimensionModeParams singleParams = DimensionMode_GetParams((DimensionMode)i);
        DimensionModeParams comboSingleParams = DimensionMode_GetComboParams(1 << i);

        if (!compare_float(singleParams.rateHz, comboSingleParams.rateHz, 1e-6f) ||
            !compare_float(singleParams.depthMs, comboSingleParams.depthMs, 1e-6f)) {
            printf("  [FAIL] Combo mask with 1 bit doesn't match authentic mode %d.\n", i + 1);
            return 1;
        }
    }

    // 2. Combination of 1+2
    DimensionModeParams combo12 = DimensionMode_GetComboParams((1 << 0) | (1 << 1));
    DimensionModeParams p1 = DimensionMode_GetParams(DIMENSION_MODE_1);
    DimensionModeParams p2 = DimensionMode_GetParams(DIMENSION_MODE_2);

    float expectedRate = (p1.rateHz > p2.rateHz) ? p1.rateHz : p2.rateHz;
    if (!compare_float(combo12.rateHz, expectedRate, 1e-6f)) {
        printf("  [FAIL] Combo rate should be max of selected.\n");
        return 1;
    }

    float expectedDepth = (p1.depthMs + p2.depthMs) * 0.7f; // Depth scaling
    if (!compare_float(combo12.depthMs, expectedDepth, 1e-6f)) {
        printf("  [FAIL] Combo depth not scaled correctly.\n");
        return 1;
    }

    // 3. All modes selected (clamp testing)
    DimensionModeParams allCombo = DimensionMode_GetComboParams(15);
    if (allCombo.depthMs > 1.5f || allCombo.mainWet > 0.4f) {
        printf("  [FAIL] Combo params not clamped correctly.\n");
        return 1;
    }

    printf("  [OK] Combo mode parameters resolve predictably.\n");
    return 0;
}

/**
 * Verify that Dimension mode 3 enables the expected voice-asymmetry configuration.
 *
 * Checks that for DIMENSION_MODE_3 the second voice parameters satisfy:
 * - baseOffset2Ms > 0.1
 * - depth2Scale < 1.0
 * - wet2Gain < wet1Gain
 * - lpf2Hz < lpf1Hz
 *
 * @returns 0 on success, 1 if any of the above parameter conditions are not met.
 */
int test_voice_asymmetry_config() {
    printf("Running Voice Asymmetry Config Test...\n");
    const DimensionModeParams p = DimensionMode_GetParams(DIMENSION_MODE_3);
    if (!(p.baseOffset2Ms > 0.1f && p.depth2Scale < 1.0f && p.wet2Gain < p.wet1Gain && p.lpf2Hz < p.lpf1Hz)) {
        printf("  [FAIL] Voice asymmetry params not configured as expected.\n");
        return 1;
    }
    printf("  [OK] Voice asymmetry parameters are active.\n");
    return 0;
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

    // Escala abusivamente para +6dB para forçar a proteção de saída
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
         printf("  [OK] Sinais contidos no estágio de proteção de saída (Max: %f)\n", max_peak);
         return 0;
    } else {
         printf("  [FAIL] Detectada anomalia de headroom! Pico medido: %f\n", max_peak);
         return 1;
    }
}

int test_stereo_width_vs_center() {
    printf("Running Stereo Width vs Center Test...\n");
    DimensionChorusState chorus;
    DimensionChorus_Init(&chorus);
    DimensionChorus_SetMode(&chorus, DIMENSION_MODE_4);

    const size_t test_frames = MONO_SUM_TEST_FRAMES;
    float* inMono = (float*)malloc(test_frames * sizeof(float));
    float* outStereo = (float*)malloc(test_frames * 2 * sizeof(float));
    if (!inMono || !outStereo) {
        printf("  [FAIL] Allocation failure.\n");
        free(inMono);
        free(outStereo);
        return 1;
    }

    generate_sine(inMono, test_frames, TEST_FREQ_1KHZ, DSP_SAMPLE_RATE);

    size_t frames_processed = 0;
    while (frames_processed < test_frames) {
        DimensionChorus_ProcessBlock(&chorus, &inMono[frames_processed], &outStereo[frames_processed * 2], DSP_BLOCK_SIZE);
        frames_processed += DSP_BLOCK_SIZE;
    }

    float sideEnergy = 0.0f;
    float centerEnergy = 0.0f;
    float inEnergy = 0.0f;
    for (size_t i = 0; i < test_frames; ++i) {
        const float l = outStereo[2 * i + 0];
        const float r = outStereo[2 * i + 1];
        const float mid = 0.5f * (l + r);
        const float side = 0.5f * (l - r);
        centerEnergy += mid * mid;
        sideEnergy += side * side;
        inEnergy += inMono[i] * inMono[i];
    }

    free(inMono);
    free(outStereo);

    const float centerRatio = sqrtf(centerEnergy / inEnergy);
    const float sideRms = sqrtf(sideEnergy / (float)test_frames);

    if (centerRatio >= CENTER_PRESERVE_MIN && sideRms >= STEREO_DIFF_MIN) {
        printf("  [OK] Center preserved (ratio=%f) with audible side spread (rms=%f).\n", centerRatio, sideRms);
        return 0;
    }

    printf("  [FAIL] Center/side balance out of expected bounds (ratio=%f, side=%f).\n", centerRatio, sideRms);
    return 1;
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

/**
 * Run the Dimension Chorus DSP core test suite.
 *
 * Executes all unit/integration tests for the Dimension Chorus module and
 * prints a summary header and the total number of failed tests.
 *
 * @returns Number of failed tests (0 if all tests passed).
 */
int main() {
    printf("=== Dimension Chorus DSP Core Tests ===\n");
    int failures = 0;
    failures += test_mode_table_ranges();
    failures += test_combo_mode_logic();
    failures += test_voice_asymmetry_config();
    failures += test_impulse_response();
    failures += test_headroom_clipping();
    failures += test_mono_summing();
    failures += test_stereo_width_vs_center();
    printf("=======================================\n");
    printf("Tests Failed: %d\n", failures);
    return failures;
}

#include "app/audio_engine.h"
#include "platform/stm32h5/audio_buffers.h"
#include "core/dimension_chorus.h"
#include <string.h>

// Buffers internos para conversão (processamento em blocos de 32 frames)
static float in_mono[AUDIO_BLOCK_FRAMES];
static float out_stereo[AUDIO_BLOCK_FRAMES * AUDIO_CHANNELS];

// Instância principal (e única na v1) do efeito DSP
static DimensionChorusState chorus_state;

/**
 * @brief Converte e desempacota RX I2S int32_t interleaved para L (mono) float32_t.
 *
 * @note Esta implementação assume que o sample RX no int32_t já está "sign-extended"
 *       para 32 bits pelo driver / hardware do PCM1808 (escala real).
 *       Se o periférico estiver configurado para 24-bit left-justified (e os 8
 *       bits LSBs não representam sign), você deve ajustar a etapa de unpack
 *       (ex: shift arithmetic ou shift right por 8 bits).
 *       A política "v1" atual extrai apenas o canal Esquerdo (L).
 *       Para migrar para média L/R com headroom, o loop deve somar ambos:
 *       in_mono[i] = (L + R) * 0.5f;
 */
static inline void UnpackRxToMonoFloat(const int32_t* rxHalfBuffer, float* outMono, size_t frames)
{
    const float norm = 1.0f / 2147483648.0f; // Escala 32 bits com sinal

    for (size_t i = 0; i < frames; i++) {
        // Pega canal L (index 0 no par interleaved L/R)
        int32_t l_sample = rxHalfBuffer[i * 2 + 0];

        // Política L apenas.
        // int32_t r_sample = rxHalfBuffer[i * 2 + 1]; // Para usar a média: (l_sample + r_sample) * 0.5 * norm;

        outMono[i] = (float)l_sample * norm;
    }
}

/**
 * @brief Converte, satura e empacota out float32_t stereo interleaved em TX I2S int32_t.
 *
 * @note Assumindo container 32-bit sign-extended pelo hardware do PCM5102.
 *       Faz clamp para evitar overflow na conversão inteira e escala em [-2147483648, 2147483647].
 *       Se left-justified de 24 bits for mandatório e o DMA de tx esperar zero-padding nos LSBs,
 *       faça bitwise masking: val & ~0xFF.
 */
static inline void PackStereoFloatToTx(const float* inStereo, int32_t* txHalfBuffer, size_t frames)
{
    // Limite ligeiramente abaixo de 1.0 para margem de float e evitar erro na conversão de inteiro de 32 bits
    const float clip_max = 0.9999999f;
    const float clip_min = -1.0f;
    const float scale = 2147483648.0f;

    for (size_t i = 0; i < frames * 2; i++) {
        float sample = inStereo[i];

        // Saturação antes da conversão para manter sinal íntegro.
        if (sample > clip_max) sample = clip_max;
        else if (sample < clip_min) sample = clip_min;

        txHalfBuffer[i] = (int32_t)(sample * scale);
    }
}

void AudioEngine_Init(void)
{
    // Inicializar DSP
    DimensionChorus_Init(&chorus_state);

    // Zerar buffers float
    memset(in_mono, 0, sizeof(in_mono));
    memset(out_stereo, 0, sizeof(out_stereo));
}

void AudioEngine_SetMode(int mode)
{
    // Filtra modos (0-3) de forma segura caso um valor inesperado venha.
    DimensionMode m;
    switch (mode) {
        case 1: m = DIMENSION_MODE_2; break;
        case 2: m = DIMENSION_MODE_3; break;
        case 3: m = DIMENSION_MODE_4; break;
        default: m = DIMENSION_MODE_1; break;
    }

    DimensionChorus_SetMode(&chorus_state, m);
}

void AudioEngine_ProcessBlock(int32_t* rxHalfBuffer, int32_t* txHalfBuffer)
{
    // 1. Unpack & Conversão RX para Mono Float L-only (v1).
    UnpackRxToMonoFloat(rxHalfBuffer, in_mono, AUDIO_BLOCK_FRAMES);

    // 2. Processamento do Chorus (Mono in -> Stereo out)
    DimensionChorus_ProcessBlock(&chorus_state, in_mono, out_stereo, AUDIO_BLOCK_FRAMES);

    // 3. Conversão & Pack Float para TX Interleaved.
    PackStereoFloatToTx(out_stereo, txHalfBuffer, AUDIO_BLOCK_FRAMES);
}

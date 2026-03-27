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
 * @brief Helper interno: Extrai áudio útil de 24 bits (left-justified) do container 32-bit.
 *        Faz o shift pra recuperar o valor com sinal em 24 bits reais.
 */
static inline int32_t AudioEngine_UnpackRx24In32(int32_t raw_32bit)
{
    // Assumimos dado de entrada 24-bit PCM left-justified (bits 31..8 úteis).
    // O shift aritmético (>>) por 8 estende o sinal de forma apropriada,
    // garantindo que os 24 bits fiquem com o sign correto em um int32_t.
    return raw_32bit >> 8;
}

/**
 * @brief Helper interno: Empacota áudio nominal 24-bit de volta pra formato left-justified 32-bit.
 */
static inline int32_t AudioEngine_PackTx24In32(int32_t sample_24bit)
{
    // Coloca os 24 bits nominais (com sinal) nos 24 MSBs do container de 32 bits,
    // preenchendo os 8 LSBs com zeros. Isso reflete o comportamento left-justified.
    return (sample_24bit << 8) & 0xFFFFFF00;
}

/**
 * @brief Converte PCM RX (container 32-bit, carga 24-bit left-justified) para Float Mono (L-only).
 *
 * @note Política v1: usa apenas o canal esquerdo (L) como entrada do DSP.
 *       Caso se deseje no futuro usar uma entrada mono baseada na soma dos dois canais,
 *       basta somar `l_sample` e `r_sample` e dividir por 2 na hora do float.
 */
static inline void AudioEngine_ConvertInputPcm32ToMonoFloat(const int32_t* rxHalfBuffer, float* outMono, size_t frames)
{
    // Escala baseada nos 24 bits reais: [-8388608, 8388607]
    const float norm = 1.0f / 8388608.0f;

    for (size_t i = 0; i < frames; i++) {
        // Pega apenas o canal L (índice 0 do par L/R)
        int32_t raw_l = rxHalfBuffer[i * 2 + 0];
        int32_t l_sample24 = AudioEngine_UnpackRx24In32(raw_l);

        outMono[i] = (float)l_sample24 * norm;
    }
}

/**
 * @brief Satura, converte float para PCM (carga 24-bit nominal) e empacota em container 32-bit TX.
 *
 * @note Assumindo saída 24-bit nominal, ajustada para "left-justified" em container de 32 bits.
 */
static inline void AudioEngine_ConvertOutputFloatToPcm32(const float* inStereo, int32_t* txHalfBuffer, size_t frames)
{
    // Fator de escala base de 24 bits: 2^23 - 1 = 8388607.0f
    const float scale = 8388607.0f;
    const float clip_max = 0.9999999f;
    const float clip_min = -1.0f;

    for (size_t i = 0; i < frames * 2; i++) {
        float sample = inStereo[i];

        // Saturação final para evitar wraps no formato inteiro
        if (sample > clip_max) sample = clip_max;
        else if (sample < clip_min) sample = clip_min;

        // Converter para a escala 24-bit int
        int32_t sample_24bit = (int32_t)(sample * scale);

        // Empacota para o container 32-bit esperado pelo DMA
        txHalfBuffer[i] = AudioEngine_PackTx24In32(sample_24bit);
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
    // 1. Unpack & Conversão RX (container 32, payload 24) para Mono Float L-only (v1).
    AudioEngine_ConvertInputPcm32ToMonoFloat(rxHalfBuffer, in_mono, AUDIO_BLOCK_FRAMES);

    // 2. Processamento do Chorus (Mono in -> Stereo out)
    DimensionChorus_ProcessBlock(&chorus_state, in_mono, out_stereo, AUDIO_BLOCK_FRAMES);

    // 3. Conversão & Pack Float para TX Interleaved (container 32, payload 24).
    AudioEngine_ConvertOutputFloatToPcm32(out_stereo, txHalfBuffer, AUDIO_BLOCK_FRAMES);
}

#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file audio_engine.h
 * @brief Engine de processamento DSP para Dimension Chorus, ponte entre Plataforma e Core.
 *
 * Responsabilidades:
 * - Conversão PCM Int32 <-> Float32.
 * - Saturação de saída (clip em [-1, 1)).
 * - Extração de canal Mono da fonte Stereo.
 * - Controle da instância do DimensionChorusState.
 */

/**
 * @brief Inicializa o DSP do Dimension Chorus e zera buffers intermediários.
 */
void AudioEngine_Init(void);

/**
 * @brief Seleciona um modo pre-definido do Dimension Chorus (0-3).
 * @param mode O índice do modo (ver DimensionMode).
 */
void AudioEngine_SetMode(int mode);

/**
 * @brief Processa uma metade de buffer vindo do DMA RX e grava no DMA TX.
 *
 * @param rxHalfBuffer Ponteiro para o início da porção correspondente do rx dma (tamanho 64 words).
 * @param txHalfBuffer Ponteiro para o início da porção correspondente do tx dma (tamanho 64 words).
 */
void AudioEngine_ProcessBlock(int32_t* rxHalfBuffer, int32_t* txHalfBuffer);

#endif // AUDIO_ENGINE_H

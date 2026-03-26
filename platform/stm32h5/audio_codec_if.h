#ifndef AUDIO_CODEC_IF_H
#define AUDIO_CODEC_IF_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file audio_codec_if.h
 * @brief Interface para configuração dos codecs de hardware (PCM1808 ADC e PCM5102 DAC).
 *
 * Premissas de Hardware:
 * - O STM32H5 atua como o Master do barramento, gerando BCLK, LRCLK/WCLK e MCLK.
 * - ADC (PCM1808) e DAC (PCM5102) funcionam no modo Slave do I2S/SAI.
 * - Taxa de amostragem configurada globalmente para 48 kHz.
 *
 * Esta camada deve gerenciar pins de Enable, Reset ou Mute (se existirem fisicamente)
 * para ligar ou desligar o caminho do som com segurança.
 */

/**
 * @brief Inicializa a ponte de codec (estado dos pinos de Mute, RST, etc).
 *
 * Se não houver GPIO de controle definido na board, este será um stub.
 * Deve ser chamado antes de inicializar o clock (SAI/I2S) e o DMA.
 */
void AudioCodec_Init(void);

/**
 * @brief Libera os codecs para operar, geralmente desativando MUTE / Standby.
 *
 * Deve ser chamado após os clocks de áudio (MCLK, BCLK, LRCLK) estarem estáveis.
 */
void AudioCodec_Start(void);

/**
 * @brief Coloca os codecs em estado seguro / MUTE.
 */
void AudioCodec_Stop(void);

#endif // AUDIO_CODEC_IF_H

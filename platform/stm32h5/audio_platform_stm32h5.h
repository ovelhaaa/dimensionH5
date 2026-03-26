#ifndef AUDIO_PLATFORM_STM32H5_H
#define AUDIO_PLATFORM_STM32H5_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file audio_platform_stm32h5.h
 * @brief Gerenciamento da ponte de áudio (SAI/I2S, DMA e blocos) no STM32H5.
 *
 * Responsabilidades:
 * - Iniciar e parar os fluxos DMA para RX e TX.
 * - Fornecer funções de callback de half e full transfer a serem chamadas
 *   pelo handler da ST (CubeMX).
 * - Controlar flags atômicas que informam ao main loop (e à Audio Engine)
 *   que há blocos prontos para o processamento em tempo real do DSP.
 */

/**
 * @brief Inicializa e prepara buffers (por exemplo, zerando-os).
 * Deve ser chamado na inicialização do sistema, antes de ativar o DMA e Codecs.
 */
void AudioPlatform_Init(void);

/**
 * @brief Dispara o DMA do periférico em modo circular.
 * No hardware real este comando chamaria HAL_SAI_Transmit_DMA e HAL_SAI_Receive_DMA.
 */
void AudioPlatform_Start(void);

/**
 * @brief Para o fluxo DMA.
 */
void AudioPlatform_Stop(void);

/**
 * @brief Chama o processamento (DSP) pendente no loop principal, se houver dado.
 * Esta função deve ser chamada constantemente no main loop da aplicação.
 * É aqui que a `AudioEngine_ProcessBlock` é disparada de forma segura e não-bloqueante,
 * longe da ISR (Interrupt Service Routine).
 */
void AudioPlatform_ProcessLoop(void);

/**
 * @brief Callback de 'Half Transfer Complete'.
 * Deve ser conectada dentro de `HAL_SAI_RxHalfCpltCallback`
 * (ou `HAL_I2S_RxHalfCpltCallback`) no código gerado pelo CubeMX.
 */
void AudioPlatform_OnRxHalfComplete(void);

/**
 * @brief Callback de 'Transfer Complete'.
 * Deve ser conectada dentro de `HAL_SAI_RxCpltCallback`
 * (ou `HAL_I2S_RxCpltCallback`) no código gerado pelo CubeMX.
 */
void AudioPlatform_OnRxFullComplete(void);

#endif // AUDIO_PLATFORM_STM32H5_H

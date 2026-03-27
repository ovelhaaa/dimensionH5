#ifndef AUDIO_APP_H
#define AUDIO_APP_H

#include "app/audio_engine.h" // Necessário para AudioPipelineMode

/**
 * @file audio_app.h
 * @brief Ponto de entrada unificado (Boot Path e Loop) da aplicação de áudio.
 *
 * Responsabilidades:
 * - Prover o caminho mínimo de inicialização do sistema completo de áudio
 *   (Engine -> Mode -> Platform -> Codec).
 * - Encapsular o loop de processamento para ser engatado facilmente no
 *   `main.c` gerado pelo CubeMX.
 */

/**
 * @brief Modo inicial do Dimension Chorus na inicialização.
 * Valor 1 corresponde ao DIMENSION_MODE_2 no switch de AudioEngine_SetMode.
 */
#define AUDIO_APP_INITIAL_MODE 1

/**
 * @brief Inicializa todo o sistema de áudio e buffers, sem ligar o transporte.
 *
 * Ordem estrita:
 * 1. Inicializa a Engine DSP.
 * 2. Define o modo inicial e o pipeline padrao (Passthrough recomendado para bring-up).
 * 3. Inicializa buffers e recursos da Plataforma.
 * 4. Inicializa os Codecs (via GPIOs de controle).
 */
void AudioApp_Init(void);

/**
 * @brief Inicia o fluxo de áudio real no hardware.
 *
 * Ordem estrita:
 * 1. Dispara o DMA (RX e TX) via HAL.
 * 2. Libera os Codecs para operação (desliga Mute/Standby).
 */
void AudioApp_Start(void);

/**
 * @brief Alterna dinamicamente entre passthrough limpo e DSP ativo.
 *
 * @param mode Modo de pipeline desejado.
 */
void AudioApp_SetPipelineMode(AudioPipelineMode mode);

/**
 * @brief Função não bloqueante para ser chamada no superloop (while 1 do main.c).
 *
 * Esta função consome as flags atômicas levantadas pelas ISRs do DMA
 * e dispara o processamento de áudio fora do contexto de interrupção.
 */
void AudioApp_ProcessLoop(void);

#endif // AUDIO_APP_H

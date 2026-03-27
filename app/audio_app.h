#ifndef AUDIO_APP_H
#define AUDIO_APP_H

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
 * @brief Inicializa todo o sistema de áudio e inicia o fluxo DMA.
 *
 * Ordem estrita:
 * 1. Inicializa a Engine DSP.
 * 2. Define o modo inicial.
 * 3. Inicializa buffers e recursos da Plataforma.
 * 4. Inicializa os Codecs (via GPIOs).
 * 5. Dispara o DMA.
 * 6. Libera os Codecs para operação.
 */
void AudioApp_Init(void);

/**
 * @brief Função não bloqueante para ser chamada no superloop (while 1 do main.c).
 *
 * Esta função consome as flags atômicas levantadas pelas ISRs do DMA
 * e dispara o processamento de áudio fora do contexto de interrupção.
 */
void AudioApp_ProcessLoop(void);

#endif // AUDIO_APP_H

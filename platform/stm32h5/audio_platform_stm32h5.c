#include "audio_platform_stm32h5.h"
#include "audio_buffers.h"
#include "app/audio_engine.h"
#include <string.h>

// Includes de HAL (ST) e abstrações do CubeMX futuramente irão aqui
// #include "stm32h5xx_hal.h"
// extern SAI_HandleTypeDef hsai_BlockA1; // TX
// extern SAI_HandleTypeDef hsai_BlockB1; // RX

/**
 * Flags atômicas (ou voláteis simples, dadas as interrupções de uma só prioridade)
 * para controle de processamento ping-pong.
 *
 * Política estrita da v1: O processamento de DSP NÃO DEVE ocorrer dentro da ISR.
 * O fluxo de execução garante que a ISR apenas levanta essas flags (muito curtas e previsíveis).
 * O main() deve consumir essas flags através da função AudioPlatform_ProcessLoop().
 */
static volatile bool rx_half_ready = false;
static volatile bool rx_full_ready = false;

void AudioPlatform_Init(void)
{
    // Zerar buffers DMA
    memset(dma_rx_buffer, 0, sizeof(dma_rx_buffer));
    memset(dma_tx_buffer, 0, sizeof(dma_tx_buffer));

    // A engine já terá que ter sido inicializada com AudioEngine_Init() na main.
}

void AudioPlatform_Start(void)
{
    // Ponto de integração dependente de handles gerados pelo CubeMX:
    // Deve ser substituído pela chamada real aos drivers ST no futuro.
    // Exemplo:
    // HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*)dma_tx_buffer, AUDIO_FULL_BUFFER_WORDS);
    // HAL_SAI_Receive_DMA(&hsai_BlockB1, (uint8_t*)dma_rx_buffer, AUDIO_FULL_BUFFER_WORDS);

    // Assegura flags zeradas
    rx_half_ready = false;
    rx_full_ready = false;
}

void AudioPlatform_Stop(void)
{
    // Ponto de integração dependente de handles gerados pelo CubeMX:
    // HAL_SAI_DMAStop(&hsai_BlockA1);
    // HAL_SAI_DMAStop(&hsai_BlockB1);
}

void AudioPlatform_ProcessLoop(void)
{
    // Esta função vive no main loop. Executa FORA da ISR.

    if (rx_half_ready) {
        rx_half_ready = false;

        // Processa a primeira metade (Half Transfer)
        // dma_rx_buffer e dma_tx_buffer começam do índice 0
        int32_t* rx_ptr = &dma_rx_buffer[0];
        int32_t* tx_ptr = &dma_tx_buffer[0];

        AudioEngine_ProcessBlock(rx_ptr, tx_ptr);
    }

    if (rx_full_ready) {
        rx_full_ready = false;

        // Processa a segunda metade (Full Transfer)
        // deslocado pelo tamanho em words de meio buffer (AUDIO_HALF_BUFFER_WORDS)
        int32_t* rx_ptr = &dma_rx_buffer[AUDIO_HALF_BUFFER_WORDS];
        int32_t* tx_ptr = &dma_tx_buffer[AUDIO_HALF_BUFFER_WORDS];

        AudioEngine_ProcessBlock(rx_ptr, tx_ptr);
    }
}

// ==============================================================================
// CALLBACKS CHAMADAS PELO HAL DA ST (e.g. stm32h5xx_it.c ou rotinas do CubeMX)
// ==============================================================================

void AudioPlatform_OnRxHalfComplete(void)
{
    // Sinaliza pro main loop que a primeira metade de dma_rx está pronta.
    // O dma_tx da primeira metade pode ser sobrescrito pelo DSP simultaneamente
    // enquanto o DMA de hardware agora foca na segunda metade.
    rx_half_ready = true;
}

void AudioPlatform_OnRxFullComplete(void)
{
    // Sinaliza pro main loop que a segunda metade de dma_rx está pronta.
    // O DMA volta do zero (modo circular) e vai encher a primeira metade
    // e enviar a primeira metade do tx.
    rx_full_ready = true;
}

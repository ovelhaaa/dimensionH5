#include "audio_codec_if.h"

// Includes de HAL (ST) e abstrações do CubeMX futuramente irão aqui
// #include "stm32h5xx_hal.h"
// #include "main.h" // Se as macros dos pinos de GPIO estiverem aqui

/**
 * @file audio_codec_if.c
 * @brief Implementação stub para as configurações do PCM1808 / PCM5102.
 *
 * Como o PCM1808 (ADC) e PCM5102 (DAC) são hardware-based (configuração por pinos),
 * muitas vezes eles não usam I2C ou SPI. Eles precisam de clocks estáveis
 * (MCLK, BCLK, LRCLK) e de sinais de Mute / XSMT.
 */

void AudioCodec_Init(void)
{
    // Exemplo do que se faria aqui na placa real:
    // HAL_GPIO_WritePin(DAC_XSMT_GPIO_Port, DAC_XSMT_Pin, GPIO_PIN_RESET); // Mute ativo
    // HAL_GPIO_WritePin(ADC_FMT_GPIO_Port, ADC_FMT_Pin, GPIO_PIN_RESET);   // Configurar formato I2S (exemplo)

    // Como os pinos exatos da placa ainda não estão definidos:
    // (Stub vazio por enquanto)
}

void AudioCodec_Start(void)
{
    // Apenas habilitar os codecs desfazendo o mute
    // Exemplo: HAL_GPIO_WritePin(DAC_XSMT_GPIO_Port, DAC_XSMT_Pin, GPIO_PIN_SET);
}

void AudioCodec_Stop(void)
{
    // Força mute
    // Exemplo: HAL_GPIO_WritePin(DAC_XSMT_GPIO_Port, DAC_XSMT_Pin, GPIO_PIN_RESET);
}

#include "audio_codec_if.h"

// PONTO DE INTEGRAÇÃO DO CUBEMX:
// 1. Incluir HAL e `main.h` para que as macros dos pinos (ex. DAC_XSMT_Pin) fiquem visíveis.
// #include "stm32h5xx_hal.h"
// #include "main.h"

/**
 * @file audio_codec_if.c
 * @brief Implementação stub para as configurações do PCM1808 / PCM5102.
 *
 * NOTA DE ARQUITETURA: Não misture "transporte serial/DMA" com "codec".
 * Como o PCM1808 (ADC) e PCM5102 (DAC) são hardware-based (configurados via pinos de GPIO),
 * eles não possuem barramento de controle I2C ou SPI. Logo, este módulo deve se
 * limitar estritamente ao controle de GPIOs de MUTE (XSMT), Reset e afins.
 * O gerenciamento do tráfego I2S / SAI e DMA pertence a `audio_platform_stm32h5.c`.
 */

void AudioCodec_Init(void)
{
    // PONTO DE INTEGRAÇÃO DO CUBEMX:
    // Configure os GPIOs para o estado inicial "seguro" (em geral, MUTE ativado ou RESET segurado).
    // O DAC PCM5102 possui o pino XSMT. O estado RESET (0) aciona o soft mute interno.
    // Exemplo:
    // HAL_GPIO_WritePin(DAC_XSMT_GPIO_Port, DAC_XSMT_Pin, GPIO_PIN_RESET);
}

void AudioCodec_Start(void)
{
    // PONTO DE INTEGRAÇÃO DO CUBEMX:
    // Habilita a saída de áudio de fato, tirando o Codec do Mute.
    // Exemplo:
    // HAL_GPIO_WritePin(DAC_XSMT_GPIO_Port, DAC_XSMT_Pin, GPIO_PIN_SET);
}

void AudioCodec_Stop(void)
{
    // PONTO DE INTEGRAÇÃO DO CUBEMX:
    // Força mute rápido para evitar pops ou clicks ao desligar a engine de áudio.
    // Exemplo:
    // HAL_GPIO_WritePin(DAC_XSMT_GPIO_Port, DAC_XSMT_Pin, GPIO_PIN_RESET);
}

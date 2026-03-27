#include "audio_codec_if.h"

// Includes de HAL (ST) e abstrações do CubeMX futuramente irão aqui
// #include "stm32h5xx_hal.h"
// #include "main.h" // Se as macros dos pinos de GPIO estiverem aqui

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
    // Coloque aqui apenas os GPIOs de hardware físico dos codecs.
    // Exemplo de placa real (MUTE acionado como init-state seguro):
    // HAL_GPIO_WritePin(DAC_XSMT_GPIO_Port, DAC_XSMT_Pin, GPIO_PIN_RESET);

    // (Stub vazio para inicialização da v1)
}

void AudioCodec_Start(void)
{
    // PONTO DE INTEGRAÇÃO DO CUBEMX:
    // Habilitar codecs desfazendo o mute.
    // Exemplo: HAL_GPIO_WritePin(DAC_XSMT_GPIO_Port, DAC_XSMT_Pin, GPIO_PIN_SET);
}

void AudioCodec_Stop(void)
{
    // PONTO DE INTEGRAÇÃO DO CUBEMX:
    // Força mute rápido.
    // Exemplo: HAL_GPIO_WritePin(DAC_XSMT_GPIO_Port, DAC_XSMT_Pin, GPIO_PIN_RESET);
}

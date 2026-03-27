#include "app/audio_app.h"
#include "app/audio_engine.h"
#include "platform/stm32h5/audio_platform_stm32h5.h"
#include "platform/stm32h5/audio_codec_if.h"

/**
 * @file audio_app.c
 * @brief Implementação do boot path e integração com o projeto CubeMX.
 *
 * Guia de Integração com CubeMX (main.c):
 * ==============================================================================
 * 1. Inclua este header no main.c:
 *    #include "app/audio_app.h"
 *
 * 2. Antes do loop infinito (mas após a inicialização dos periféricos SAI/DMA), chame:
 *    AudioApp_Init();
 *
 * 3. Dentro do loop infinito `while (1)`, chame contínuamente:
 *    AudioApp_ProcessLoop();
 *
 * 4. Nos callbacks do HAL em `stm32h5xx_it.c` ou equivalente gerado, faça o repasse:
 *    void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
 *        AudioPlatform_OnRxHalfComplete();
 *    }
 *    void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
 *        AudioPlatform_OnRxFullComplete();
 *    }
 * ==============================================================================
 */

void AudioApp_Init(void)
{
    // 1. Iniciar o Core DSP e buffers internos
    AudioEngine_Init();

    // 2. Definir o modo inicial fixo da v1 (exemplo: Modo 1)
    AudioEngine_SetMode(1);

    // 3. Inicializar a Plataforma (Zerar buffers DMA RX/TX)
    AudioPlatform_Init();

    // 4. Configurar HW do Codec (RST, MUTE pinos - se aplicável)
    AudioCodec_Init();

    // 5. Iniciar fluxo de DMA Circular RX e TX
    AudioPlatform_Start();

    // 6. Tirar o Mute e liberar para áudio
    AudioCodec_Start();
}

void AudioApp_ProcessLoop(void)
{
    // Consome as flags levantadas pela ISR do DMA e processa um block por vez.
    AudioPlatform_ProcessLoop();
}

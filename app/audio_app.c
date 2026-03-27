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
 *    AudioApp_Start();
 *
 * 3. Para testes de hardware, o pipeline inicia em PASSTHROUGH. Para ligar o DSP:
 *    AudioApp_SetPipelineMode(AUDIO_PIPELINE_DSP);
 *
 * 4. Dentro do loop infinito `while (1)`, chame contínuamente:
 *    AudioApp_ProcessLoop();
 *
 * 5. Nos callbacks do HAL em `stm32h5xx_it.c` ou equivalente gerado, faça o repasse:
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

    // 2. Definir o modo inicial fixo da v1 (Chorus Mode 2) e Pipeline Segura
    AudioEngine_SetMode(AUDIO_APP_INITIAL_MODE);
    AudioEngine_SetPipelineMode(AUDIO_PIPELINE_PASSTHROUGH); // Bring-up inicial

    // 3. Inicializar a Plataforma (Zerar buffers DMA RX/TX)
    AudioPlatform_Init();

    // 4. Configurar HW do Codec (RST, MUTE pinos - se aplicável)
    AudioCodec_Init();
}

void AudioApp_Start(void)
{
    // 1. Iniciar fluxo de DMA Circular RX e TX pelo hardware real
    AudioPlatform_Start();

    // 2. Tirar o Mute e liberar para o fluxo de áudio rodar limpo
    AudioCodec_Start();
}

void AudioApp_SetPipelineMode(AudioPipelineMode mode)
{
    AudioEngine_SetPipelineMode(mode);
}

void AudioApp_ProcessLoop(void)
{
    // Consome as flags levantadas pela ISR do DMA e processa um block por vez.
    AudioPlatform_ProcessLoop();
}

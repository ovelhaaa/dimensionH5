# Guia de Integração CubeMX: Dimension DSP (STM32H5)

Este documento detalha exatamente como integrar a ponte de áudio criada neste repositório ao código gerado automaticamente pelo **STM32CubeMX**, garantindo uma interface não bloqueante com SAI/I2S, DMA, e o ADC/DAC externo.

## 1. Configuração Básica no CubeMX
*   **Clock de Áudio:** Configure o PLL de áudio para atingir exatamente **48 kHz** na interface SAI/I2S.
*   **Interface:** Configure SAI ou I2S como **Master** com tamanho de dado **32 Bits** em frame de 32 Bits.
*   **DMA:** Ative modo **Circular** tanto para TX quanto para RX e habilite interrupções associadas ao final e metade do tráfego (Half e Full transfer).
*   **Prioridade:** Atribua prioridade segura para o DMA sem que sufoque interrupções críticas do sistema (ex: SysTick).

## 2. Inclusões no `main.c` do CubeMX
No bloco `/* USER CODE BEGIN Includes */` do seu `main.c`, inclua os headers necessários:
```c
#include "app/audio_engine.h"
#include "platform/stm32h5/audio_platform_stm32h5.h"
#include "platform/stm32h5/audio_codec_if.h"
```

## 3. Inicialização e Seleção de Modo Inicial
Ainda no `main.c`, mas na função `main()`, dentro de `/* USER CODE BEGIN 2 */`, adicione a rotina de boot:
```c
    // 1. Inicializa o Audio Engine (DSP puro, estados e buffers internos)
    AudioEngine_Init();

    // 2. Opcional: define um modo fixo de Chorus (v1: 0 a 3)
    AudioEngine_SetMode(0);

    // 3. Prepara a ponte da plataforma (zera buffers DMA rx/tx estáticos)
    AudioPlatform_Init();

    // 4. Se a sua placa possuir pinos de Reset/Mute do PCM1808 e PCM5102, trate eles aqui
    AudioCodec_Init();
    AudioCodec_Start(); // Libera o hardware de áudio

    // 5. Inicia o Hardware de SAI/I2S e DMA (ATENÇÃO: Este passo deve ser adaptado à sua placa real)
    // AudioPlatform_Start(); // Na versão final com placa, você pode colocar o HAL_SAI_Receive_DMA lá dentro
    // ou apenas chamá-los aqui:
    // HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*)dma_tx_buffer, AUDIO_DMA_BUFFER_WORDS);
    // HAL_SAI_Receive_DMA(&hsai_BlockB1, (uint8_t*)dma_rx_buffer, AUDIO_DMA_BUFFER_WORDS);
```

## 4. O Superloop Principal (Processamento DSP Real)
O DSP nunca deve rodar dentro da ISR do hardware (para evitar bloqueios). O DSP vai processar cada 32 frames (metade do DMA buffer) continuamente no `while(1)`. Adicione isso ao final de `main.c`, no bloco `/* USER CODE BEGIN 3 */`:
```c
    while (1)
    {
        // ... sua lógica UI, se existir ...

        // Processa áudio pendente (se os buffers estiverem prontos pelas flags da ISR)
        AudioPlatform_ProcessLoop();
    }
```

## 5. Ligação das Callbacks HAL (no `stm32h5xx_it.c` ou `main.c`)
As interrupções do SAI/I2S devem avisar a plataforma. Localize as definições de callback do CubeMX (`HAL_SAI_RxHalfCpltCallback` e `HAL_SAI_RxCpltCallback`) e chame a ponte de sinalização interna:
```c
/* USER CODE BEGIN 4 */
void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    // Verifica qual instância disparou, se necessário
    AudioPlatform_OnRxHalfComplete();
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
    // Verifica qual instância disparou, se necessário
    AudioPlatform_OnRxFullComplete();
}
/* USER CODE END 4 */
```

## O que está pendente para o Hardware Real
*   No arquivo `audio_platform_stm32h5.c`, a função `AudioPlatform_Start()` e `AudioPlatform_Stop()` possuem chamadas HAL comentadas. Na integração real, elas podem usar as variáveis globais `hsai_BlockX` definidas pelo CubeMX.
*   Em `audio_codec_if.c`, pinos como o `XSMT` do PCM5102, que tiram o conversor do Mute, precisam ter seus nomes macros ajustados.

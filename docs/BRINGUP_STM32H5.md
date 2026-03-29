# Guia de Bring-up de Áudio: STM32H5 + PCM1808/PCM5102

Este documento descreve as etapas exatas para o primeiro teste real de áudio na placa usando o código de firmware atual. O objetivo principal do bring-up é validar a via de dados bruta (clocks, empacotamento, DMA) usando o **Passthrough Limpo**, e em seguida ativar o **Processamento DSP**.

---

## 1. Ordem de Bring-up Físico

A integração do hardware no firmware deve ocorrer na seguinte ordem:

1. **Clocks do Barramento (PLL, SAI/I2S):** Garantir que o master clock (MCLK) e a taxa de 48kHz (BCLK/LRCLK) estão perfeitamente configurados pelo CubeMX.
2. **Inicialização do DMA Circular e SAI/I2S:** Configurar buffers de hardware e alinhar chamadas HAL.
3. **Validação do Passthrough (RX/TX sem DSP):** Ligar o pipeline no modo `AUDIO_PIPELINE_PASSTHROUGH` para verificar transparência. O sinal que entra no ADC deve sair limpo no DAC.
4. **Ativação do DSP:** Mudar para `AUDIO_PIPELINE_DSP` e injetar áudio mono-in / stereo-out processado.

---

## 2. Pontos de Integração com o CubeMX

### 2.1. main.c
* **Include:** `#include "app/audio_app.h"`
* **Boot Path (Antes do `while (1)`):**
  ```c
  AudioApp_Init();
  // Para testar DSP direto, pode usar AudioApp_SetPipelineMode(AUDIO_PIPELINE_DSP);
  AudioApp_Start();
  ```
* **Superloop (Dentro do `while (1)`):**
  ```c
  AudioApp_ProcessLoop();
  ```

### 2.2. audio_platform_stm32h5.c
* **Handles do Periférico:**
  Declarar os handles gerados pelo CubeMX que representam o SAI/I2S para os barramentos RX e TX. Ex: `extern SAI_HandleTypeDef hsai_BlockA1;`
* **Iniciando DMA (`AudioPlatform_Start`):**
  Disparar o DMA de recepção e transmissão usando `HAL_SAI_Receive_DMA()` e `HAL_SAI_Transmit_DMA()`.

### 2.3. stm32h5xx_it.c (Interrupções)
* As interrupções de DMA do SAI ou I2S devem repassar as callbacks.
  ```c
  void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
      if(hsai->Instance == SAI1_Block_B) { // Ajuste para a sua sub-block RX
          AudioPlatform_OnRxHalfComplete();
      }
  }

  void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
      if(hsai->Instance == SAI1_Block_B) {
          AudioPlatform_OnRxFullComplete();
      }
  }
  ```

### 2.4. audio_codec_if.c
* **GPIO de Mute/Reset:**
  Usar funções HAL de pino de GPIO em `AudioCodec_Init()`, `AudioCodec_Start()`, e `AudioCodec_Stop()`. O DAC PCM5102 usa um pino XSMT (`RESET` aciona mute, `SET` libera áudio).

---

## 3. Riscos Conhecidos e Hipóteses de Projeto

O principal ponto de fragilidade no primeiro teste em hardware real diz respeito ao comportamento serial bit a bit em oposição à memória física do STM32:

* **Alinhamento do Empacotamento SAI/I2S:**
  * A **hipótese atual (v1)** assume que a taxa de dados está chegando do DMA como **PCM 24-bit Left-Justified** dentro de um container Int32. Isso significa que extraímos os bits via descolamento de registro `>> 8` (veja as funções `AudioEngine_UnpackRx24In32` e `AudioEngine_PackTx24In32`).
  * **Risco:** O CubeMX pode estar configurado para gerar/aceitar _Right-Justified_ e o sinal ser interpretado incorretamente, soando como chiado, silêncio extremo (gain baixo) ou dc-offset brutal. A correção requer remover/alterar os bitshifts.
* **Tamanho de Word / Comprimento de Slot:** Garantir configuração do periférico gerando frames de 32 bits, caso contrário o alinhamento do `int32_t` nos arrays `dma_rx_buffer` fará leitura cruzada L/R no mesmo elemento.
* **Ordem de Canais (Swap L/R):** Algumas topologias invertem os canais L e R, o que pode mascarar o mono-in L-only que a v1 pressupõe.
* **Silêncio Fatal:** Se as chamadas `HAL_SAI_Receive_DMA` forem chamadas após a fonte de clock ficar instável ou demorar, o callback `HalfCplt` nunca disparará.

---

## 4. Sequência de Teste de Sucesso

### Teste A: Passthrough
1. Pipeline setado em `AUDIO_PIPELINE_PASSTHROUGH`.
2. Injetar onda senoidal mono 1kHz (Line in/Instrument level) no ADC L e R.
3. Observar com osciloscópio (ou placa de som) o DAC L e R.
4. **Critério de sucesso:** Onda perfeita, mesma amplitude, sem glitching, sem aliasing, sem inversão de fase estranha e nenhum clique audível.

### Teste B: Processamento (DSP)
1. Mudar código de `AUDIO_PIPELINE_PASSTHROUGH` para `AUDIO_PIPELINE_DSP`.
2. Injetar a mesma senoidal mono (L).
3. **Critério de sucesso:** Saída em estéreo, modulada e alargada, comprovando o envio e retorno correto de floats no ciclo rápido dentro da CPU (SRAM e FPU), dentro do limite de tempo (`AUDIO_BLOCK_FRAMES` processados antes da próxima flag).

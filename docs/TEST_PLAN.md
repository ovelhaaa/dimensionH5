# TEST PLAN V1

Este plano orienta a validação estruturada do Dimension Chorus antes da homologação final no STM32H5. A prioridade atual é estabilidade host/offline (redução de risco de processamento) antes do Hardware Bring-up.

## 1. Testes Automatizados (Suite Unitária/Integração DSP)
Implementados em `tests/test_dsp_core.c` via CMake/CTest.

### 1.1 Impulse Response (Timing e Fase)
- **Objetivo**: Injetar um impulso de Dirac (`1.0` na amostra 0, seguido de `0.0`).
- **Por que é prioritário?**: Valida os offsets fracionários dos taps de delay. Garante que os atrasos não estão com "off-by-one" ou comportamentos ruidosos de leitura de buffer na Hermite Interpolation.

### 1.2 Sine THD e Clipping de Headroom
- **Objetivo**: Injetar onda senoidal pura em `1.0f` (Full Scale) em 440 Hz ou 1 kHz.
- **Por que é prioritário?**: Valida que a topologia biquad (DF2T) está estável sem estourar mantissas, confirmando que as variações nos gains de dry/wet dentro do roteamento do Dimension Chorus não ceifam o sinal de maneira digital ríspida (hard clipping) nas somas e que as etapas de saturação da `audio_engine.c` e `dsp_math` (Soft Clip) estão operando devidamente.

### 1.3 Mono Sum (Cancelamento de Fase Estéreo)
- **Objetivo**: Processar uma entrada estéreo com o efeito ligado, e no final, somar L + R do buffer de saída (`0.5 * (outL + outR)`).
- **Por que é prioritário?**: O efeito Dimension depende criticamente de matrizes estéreo e fases invertidas em taps para a largura estéreo. Um erro no LFO ou sinal trocado vai anular completamente as baixas frequências do áudio (Low-end loss) quando sumado para mono. A métrica avalia que o root mean square (RMS) não caia a níveis destrutivos e avalia o HPF cruzado.

## 2. Validação Audível (Runner Offline C)
Executar `tests/offline_runner` para processamento direto do código `core` usando um arquivo `.wav`.
- **White/Pink Noise**: Verifica estabilidade de espectro sem ressonâncias anômalas.
- **Log Sweep**: Garante transição contínua entre todas as frequências baixas às agudas no efeito.
- **Audio com variação de Mode**: Avaliar estalos e pops que possam acontecer ao fazer `DimensionChorus_SetMode()` enquanto o bloco de áudio flui. É crucial implementar crossfading se ruídos severos forem detectados.

## 3. Hardware Bring-up STM32H5 (Próxima Fase)
- **Passthrough puro**: Validar a cadeia I2S -> DMA -> I2S limpa sem o processador para atestar clocks (48kHz) em MCLK/BCLK e o empacotamento 24-in-32 bit.
- **Callback Integrity e MPU**: Checar que `HAL_SAI_RxHalfCpltCallback` bate num limite temporal seguro do Cortex-M33 (verificando margens no osciloscópio por GPIO Toggle) usando buffers de áudio alocados especificamente numa zona não-cacheável (SRAM3/AXI com D-Cache OFF).

## 4. Acceptance Criteria
- 100% testes CTest OK no PC Host Linux/Ubuntu do CI.
- Runner offline gerando saídas WAV sem artefatos em todos os modos (0, 1, 2, 3 e multi).
- Zero buffer overruns ou DMA Error Callbacks disparados na firmware da board física.
- Sem distorção aparente no ouvido ao monitorar via STM32H5 DAC de saída.
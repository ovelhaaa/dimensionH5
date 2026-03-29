# Plano de Execução (V1)

## 1. Estrutura de Diretórios Recomendada (Neste Momento)
```text
.
├── core/                # Código DSP puro (100% C99, testável no PC, sem STM32)
│   ├── dimension_chorus.c/h # Core DSP (delay, LFOs, roteamento estéreo)
│   ├── dimension_modes.c/h  # Definição e configuração dos 4 botões/modos
│   ├── dsp_biquad.c/h       # Filtros (DF2T) usados no LPF e HPF
│   ├── dsp_interp.c/h       # Hermite interpolation para leitura do delay
│   └── dsp_math.h           # Macros (não include de <math.h>) e configs
├── app/                 # Camada de Aplicação (Ponte entre Target e Core)
│   └── audio_engine.c/h     # Inicializa o State, processa blocos (Int32 <-> Float) e satura a saída
├── platform/            # Código Hardcore de MCU (CubeMX, BSP, Drivers)
│   └── stm32h5/         # Inicialização do SAI, DMA, Clocks e MPU
├── tests/               # Ambientes de Host para validação (CMake)
│   ├── CMakeLists.txt
│   ├── test_main.c          # Ponto de entrada (runner custom ou Unity/CMocka)
│   ├── test_dsp_core.c      # Suite de testes para validação matemática do DSP
│   └── offline_runner.c     # Processador WAV offline (smoke test audível)
├── docs/                # Artefatos do ciclo de vida
│   ├── EXECUTION_PLAN.md
│   ├── TEST_PLAN.md
│   └── V1_SCOPE.md
└── CMakeLists.txt       # Build master (Apenas para host testing neste momento!)
```

## 2. Próximos Arquivos a Criar
A base (`core` e `app`) já começou a ser gerada. Para formalizar e focar em execução imediata sem ir para a placa ainda, os *próximos* devem ser:

1. **`tests/test_dsp_core.c`**
   - **Objetivo**: Testar objetivamente os comportamentos cruciais para um chorus (tempo, distorção, cancelamento de fase).
   - **Conteúdo Mínimo**:
     - Teste de Headroom (Injetar valores em `+-1.0f` e checar saturação soft).
     - Teste de Cancelamento de Fase em Mono Summing.
     - Teste com Impulso de Dirac para conferir taps de delay.
   - **Dependências**: `test_main.c`, `core/dimension_chorus.h`

2. **`tests/offline_runner.c`**
   - **Objetivo**: Fazer o que a placa fará, mas offline lendo de arquivo WAV e cuspindo outro arquivo WAV processado para audição humana (sanity check real).
   - **Conteúdo Mínimo**:
     - Loop de leitura de `[block_size]` amostras float de arquivo in.wav.
     - Chamada `DimensionChorus_ProcessBlock`.
     - Escrita de arquivo out.wav para inspecionar artefatos, estalos e funcionamento do delay.
   - **Dependências**: Uma lib de leitura WAV simples (ou baseada em dr_wav) e `core/dimension_chorus.h`.

## 3. Ordem Ideal de Implementação
1. **Runner Offline (`tests/offline_runner.c`)**: Fundamental para podermos ouvir as iterações imediatamente antes de debugar os números.
2. **Suite DSP Objetiva (`tests/test_dsp_core.c`)**: Evitar regressões conforme o DSP evolui.
3. **Plataforma STM32 (`platform/stm32h5/*`)**: Integrar os callbacks do STM32 com o `app/audio_engine.c`.

## 4. Runner Offline e Integração CMake
O `CMakeLists.txt` raiz não se misturará com o Makefile/CMake do CubeMX da placa. Ele trata **estritamente** de gerar artefatos host de `core`, `app` e `tests`.

O novo `offline_runner` funcionará lendo um arquivo raw ou WAV simplificado (para evitar trazer dependências complexas neste ponto). Faremos os testes via CTest.

## 5. O Que NÃO Fazer Agora
1. **Não tocar na pasta `platform/stm32h5/`**: Fazer hardware bringup (I2S, MPU, Cache, STM32Cube) enquanto o core pode ter falhas de DSP ou memória vai misturar dois domínios difíceis de debugar (Hardware Hard Faults vs Audio Glitches). O DSP deve ser aprovado em PC (Mac/Linux) no `tests/offline_runner` primeiro.
2. **Não usar `malloc` ou VLA em cantos novos do código**: STM32 precisa de memória em seções fixas não-cacheáveis (D-Cache D3). Tudo precisa continuar com alocação estática ou alocado pelo chamador.
3. **Não integrar UI ou controle MIDI**: Somente controle hardcoded `SetMode(0..3)` por enquanto. Foco total em passar áudio limpo.
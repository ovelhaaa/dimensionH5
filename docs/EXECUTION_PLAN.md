# Execution Plan

## Phase 1: Validar Offline no Desktop (Semana 1)
- Limpar base: implementar as baterias de `CTest` objetivas (Impulso, Ruído, Mono, etc).
- Testar intensamente via `render_wav` e usar escuta para refinar o algoritmo.
- Ativar GitHub Actions para CTest no host e upload dos artefatos `.wav`.

## Phase 2: Testes de Stress, Limites e Medições (Semana 2)
- Adicionar as proteções de assertions em tempo de compilação `_Static_assert`.
- Auditar transições de modo e recalibrar a constante de "smoothing" de bloco se houver zípers.
- Implementar perfis de profiling estático (contar per-sample operations).

## Phase 3: Bring-up no Hardware STM32H5 (Semana 3)
- Configurar CubeMX (Clocks, SAI/I2S, DMA).
- Executar passthrough limpo e verificar alinhamento com osciloscópio (sem rodar código DSP).
- Parametrizar a MPU para o D-Cache lidar graciosamente com buffers DMA.
- Ativar o DSP Mode e rodar medição com GPIO toggle para perfilar latência em tempo real e folga na CPU.

## Phase 4: Refinamento Sonoro (Semana 4)
- Com o hardware montado e injetando som, reavaliar Ganhos e Filtros. Ajustar ganhos de base (`dryGain`, main, cross).
- Escutar artefatos.

## Phase 5: Empacotamento Open Source / Produto (Semana Final)
- Incorporar rotinas de Debounce para UI (Chaves de Modo, Foot-Switch bypass).
- Organizar documentação (Esquemas de build flash ARM, placa).
- Preparar tag de release v1.0.
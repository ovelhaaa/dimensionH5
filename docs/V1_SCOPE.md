# V1 Scope

O escopo atual representa a primeira iteração MVP focada no Core DSP rodando de forma isolada do hardware antes de unificar na placa.

## In scope (Neste momento)
- Pipeline estéreo Dimension-style chorus funcional e validado.
- Estrutura completa 100% C99 sem uso de `<math.h>` na hot path (por frame).
- 4 preset modes baseados em modulações e misturas lógicas do equipamento clássico.
- Arquitetura estrita DMA -> Engine Float -> DSP Core -> Engine Int32 -> DMA.
- Desktop WAV render path offline (`tests/offline_runner.c`) sem lib externa monstruosa (apenas lib leve .wav).
- Suite automatizada de validação comportamental (`tests/test_dsp_core.c`) para Headroom, Timing e Phase Cancellation (Mono-compatibility).

## Out of scope (Desvio de Foco)
- Tocar em configurações do CubeMX (Clocks HSI/HSE/PLL, SAI, DMA) ou integrar Drivers ST HAL no projeto ainda (isto pode quebrar a confiança no core num momento frágil).
- Display UI para STM32H5.
- Entrada MIDI.
- Preset storage (Flash / EEPROM).
- Multi-effect engine modular abstrato.
- Advanced editor via USB/Serial.
- Otimizações extremas de Cortex-M33 assembly/SIMD. Primeiro faça rodar bem e limpo em float puro (C99).
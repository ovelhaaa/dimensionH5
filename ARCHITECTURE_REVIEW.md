# Relatório de Avaliação Arquitetural - DimensionH5

## Visão Geral
O projeto DimensionH5 é uma implementação promissora e sólida de um chorus estéreo estilo Roland Dimension focado no microcontrolador STM32H5. O repositório já demonstra maturidade ao separar a lógica DSP (agnóstica de hardware) em `core/`, a gestão do ciclo de vida e adaptação de formatos no `app/` e a abstração de comunicação de hardware em `platform/`.

O objetivo de transformá-lo em um pedal/produto ou open-source de referência é perfeitamente exequível. O processamento block-based por DMA operando fora de interrupções, suportado por float32 a 48 kHz de forma nativa pela FPU do M33, provê um excelente equilíbrio entre clareza de código e performance.

---

## Diagnóstico Técnico

### O que está bem encaminhado (Manter):
- **Topologia DSP:** O uso de 1 buffer circular com 2 vozes moduladas (taps) lidas em contra-fase nominal reproduz de forma fiel a estabilidade espacial e a modulação discreta do chorus *Dimension* sem abusar da RAM.
- **Técnicas e Otimizações:** Interpolação Hermite (3ª ordem) é o sweet spot ideal para Cortex-M33 (muito superior ao linear e menos custoso do que sinc).
- **Voicing Seguro:** Uso de biquads DF2T para o HPF e LPF no sinal molhado (wet).
- **Gerenciamento LFO:** O uso de acumulador de fase com triângulo filtrado por polo-único gera uma modulação musical sem os trancos comuns em LFOs brutos.
- **Separação de Preocupações:** Os testes isolados de host (`render_wav`, executáveis CTest) são o pilar de um DSP bem projetado.

### O que deve ser refatorado:
- **Rotinas de PCM Packing/Unpacking:** As funções `AudioEngine_UnpackRx24In32` confiam em *bitshifts* presumindo alinhamento left-justified de 24 bits. Isso precisa ser comprovado no teste de passthrough da placa real (I2S vs DMA alignment).
- **Manejo de Buffer Negativo (Delay):** A verificação de posições fracionárias negativas de leitura (`if (readPos < 0) readPos += SIZE`) foi incluída mas precisa ser protegida por asserts de buffer em potência de 2 para eficiência em bitwise ANDs na função Hermite.
- **Limpeza do Hot Path:** Quaisquer laços remanescentes de biblioteca padrão `<math.h>` no block processing devem ser erradicados (fazer caching prévio).
- **Interface de Inspeção:** As saídas volumosas do `ProcessBlock_Inspect` poluem temporariamente a cache. Devem ser controladas firmemente no build do STM32 (idealmente extirpadas por macros para uso exclusivo no Desktop).

### O que deve ser descartado:
- Qualquer lógica de codec ou de inicialização periférica de HW que tente vazar para o módulo `audio_app` ou `audio_engine`.
- Constantes ou buffers declarados como números mágicos; centralizar no `audio_buffers.h`.
- Operações de `malloc()` (projeto já evita, e assim deve continuar rigidamente).

---

## Riscos (STM32H5 Real-Time)

1. **Alinhamento do Protocolo I2S/SAI vs DMA:** Incompatibilidades no tamanho do slot (32-bit vs 24-bit vs 16-bit) entre o Codec e o STM32. Errar o bit shift transforma o áudio em offset DC monstruoso ou ruído estático destrutivo.
2. **Coerência de Cache (D-Cache):** O STM32H563 provê cache. Variáveis globais mapeadas no DMA (`dma_rx_buffer` / `tx`) e acessadas pelo app devem obrigatoriamente viver em regiões de memória Non-Cacheable (configurado pela MPU), ou exigirão rotinas de invalidação/limpeza de cache explícitas antes/depois das chamadas do DSP. Omitir isso causará leitura de ruídos ou repetição de blocos velhos (Glitch).
3. **Miss de Deadline (Superloop Blocked):** Como o processamento DSP está no loop principal consultando os flags do callback DMA, qualquer subrotina (ex: varrer I2C, displays, UI) que bloqueie por >0.6ms resultará num gap audível (dropout) estourando o prazo do próximo bloco do DMA.

---

## Estrutura de Pastas Sugerida para Crescimento

A estrutura atual é boa, mas precisa de uma hierarquia que abrace automações de teste, drivers de target e documentações dispersas:

```text
dimensionH5/
├── core/                   # DSP Puro, float32, math operations (100% agnóstico)
├── app/                    # Parâmetros, Preset management e ponte PCM<->Float
├── platform/               # BSP
│   ├── stm32h5/            # STM32 HAL Configs, Core I/O, DMA, MPU cache
│   └── hardware_tests/     # Código descartável/útil só para bring-up de placa
├── tests/                  # Testes Unitários e Integração (CMake Host-side)
│   ├── unit/               # biquad, interpolação, LFO boundaries
│   ├── integration/        # áudio engine mocks
│   └── output/             # Artefatos gerados pelo `render_wav`
├── docs/                   # Todos os MDs (Technical Audit, Specs, Manuais)
├── scripts/                # Automações (Python para plotar FFT, CI helpers)
├── CMakeLists.txt
└── AGENTS.md
```

---

## Proposta de Testes Objetivos

1. **Impulso (Impulse Response):** Passar delta de Dirac (1 e sequência de zeros) para validar os tempos de delay exatos e garantir a estabilidade global dos biquads.
2. **Sweep de Seno (Sine Sweep):** Avaliar THD+N e confirmar a ação do Low Pass (corte > 6kHz) de cada tap.
3. **Seno Fixo (Headroom & Clipping):** Inserir onda 1kHz a +2dBFS (ex: valores = 1.25f) para avaliar o soft clipper polinomial em condições extremas e certificar que não trava (NaNs).
4. **Ruído Branco:** Avaliar o balanço global de RMS In/Out. Testar que os 4 modos mantêm um ganho aparente ("unity gain") unificado.
5. **Troca de Modo (Mode Switch):** Forçar transição entre `DIMENSION_MODE_1` e `DIMENSION_MODE_4` com sinal rodando e capturar a saída para verificar se há artefato audível (zipper noise, clicks ou estouro da biquad re-calculando).
6. **Mono Compatibility:** Operar com a função estéreo padrão e fazer um render que some OutL e OutR (soma -6dB). O sinal resultante não pode ter vales de comb-filter fixos agressivos devido aos taps independentes não-espelhados e à polaridade da matriz de mixagem L/R original.

---

## Roadmap Prático em Fases (30 Dias)

- **Fase 1: Validar Offline no Desktop (Semana 1)**
  - Limpar base: implementar as baterias de `CTest` objetivas (Impulso, Ruído, Mono, etc).
  - Testar intensamente via `render_wav` e usar escuta para refinar o algoritmo.
  - Ativar GitHub Actions para CTest no host e upload dos artefatos `.wav`.

- **Fase 2: Testes de Stress, Limites e Medições (Semana 2)**
  - Adicionar as proteções de assertions em tempo de compilação `_Static_assert`.
  - Auditar transições de modo e recalibrar a constante de "smoothing" de bloco se houver zípers.
  - Implementar perfis de profiling estático (contar per-sample operations).

- **Fase 3: Bring-up no Hardware STM32H5 (Semana 3)**
  - Configurar CubeMX (Clocks, SAI/I2S, DMA).
  - Executar passthrough limpo e verificar alinhamento com osciloscópio (sem rodar código DSP).
  - Parametrizar a MPU para o D-Cache lidar graciosamente com buffers DMA.
  - Ativar o DSP Mode e rodar medição com GPIO toggle p/ perfilar latência em tempo real e folga na CPU.

- **Fase 4: Refinamento Sonoro (Semana 4)**
  - Com o hardware montado e injetando som, reavaliar Ganhos e Filtros. Ajustar ganhos de base (`dryGain`, main, cross).
  - Escutar artefatos.

- **Fase 5: Empacotamento Open Source / Produto (Semana Final)**
  - Incorporar rotinas de Debounce para UI (Chaves de Modo, Foot-Switch bypass).
  - Organizar documentação (Esquemas de build flash ARM, placa).
  - Preparar tag de release v1.0.

---

## Próximas 10 Ações Mais Importantes (Ordem de Execução)

1. Mover todos os arquivos `.md` (exceto README/AGENTS) soltos na raiz para a pasta `docs/`.
2. Incluir `_Static_assert` para garantir que o tamanho do buffer circular é potência de 2.
3. Construir os arquivos do CMake test framework focando primeiro no teste "Impulse" (dirac delta) no DSP e "Mono-compat" mode.
4. Auditar todo o hot path (`DimensionChorus_ProcessBlock`) e garantir que não restam chamadas como `expf`, `sinf` ou `tanf` por amostra.
5. Iniciar setup de GitHub Actions com a stack de GCC, build com CTest + upload WAV artifacts.
6. Isolar o código do gerador da ST de forma modular na pasta `platform/stm32h5/` não poluindo arquivos do núcleo ou do app.
7. Adicionar setup explícito de D-Cache da MPU (Non-Cacheable SRAM region) aos buffers `dma_rx` e `tx`.
8. Implementar um teste Passthrough (inverte o buffer 32 rx para 32 tx direto) e usar um osciloscópio para validar o bit shift de 24 em 32-bits na MCU viva antes de acionar DSP.
9. Implementar pino de Profile GPIO Toggle na entrada e saída de `AudioEngine_ProcessBlock` para monitorar carga CPU e garantir ausência de deadline misses.
10. Refinar transições de modo usando um fade-out/fade-in curto ou recalculando apenas targets suavizados em background para mitigar artefatos IIR durante troca.
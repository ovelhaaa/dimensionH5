# Relatório de Validação Offline - Dimension Chorus Core

## Resumo das Ações
Nesta rodada, foi criada uma camada de validação offline ("host-side") para testar e verificar o núcleo DSP do Dimension Chorus sem depender do ecossistema STM32 HAL.

1. **Harness de Testes (C/CMake):** Foi criada a pasta `tests/` e um sistema de build CMake foi configurado. Testes básicos foram implementados (`test_dimension_core.c`, `test_interp.c`, `test_biquad.c`) garantindo que a inicialização ocorre sem lixo na memória, que as trocas de modo carregam os targets corretamente, que os coeficientes biquad não produzem NaN/Inf e que a matemática de bounds está íntegra.
2. **Renderização de WAV:** Foi criado um renderizador puramente em C (`wav_writer.c` e `render_wav_main.c`) capaz de exportar arquivos 16-bit PCM estéreo a 48 kHz. Um sinal de teste contendo um transiente seguido por uma varredura senoidal foi criado para excitar as modulações dos 4 modos (gerando `mode_1.wav` a `mode_4.wav`). Isso atende o requisito de ter arquivos para inspeção auditiva e visual (ex: no Audacity).

## Achados e Revisões no Core

### 1. Comportamento do SoftClip
A função `Dsp_SoftClip` utiliza a aproximação $f(x) = x - x^3/3$.
*   **Comportamento Linear:** Para valores pequenos de $x$ (ex: $\pm0.3$), a função é praticamente linear. Em $x = 0.5$, a saída é $0.458$, apresentando leve compressão.
*   **Pico Máximo e Clamp:** Em $x = 1.0$, a saída atinge $2/3$ ($\approx 0.6667$). Qualquer entrada $|x| \ge 1.0$ resulta em um hard clamp travado em $\pm 2/3$.
*   **Conclusão para v1:** Esse limite rígido de aproximadamente -3.5dBFS não satura até o 0dBFS absoluto do DAC, mas provê uma margem de segurança *cheap* e eficaz. Modificar isso para escalar até 1.0 custaria processamento no hot-path ou sacrificaria a margem de headroom do voicing atual (onde ganhos já estão balanceados ao redor do center dry). Foi adicionada documentação detalhando essas características diretamente em `core/dsp_math.h` e a lógica original foi preservada por ser determinística e extremamente segura.

### 2. Convenção e Limites do `readPos`
A lógica de leitura da linha de delay no `dimension_chorus.c` e a interpolação em `dsp_interp.c` foram revisadas.
*   **Convenção:** O atraso (`tapTime`) é computado em "samples de distância" e é sempre subtraído do ponteiro atual de escrita (`writeIndex`), garantindo que o delay de áudio represente amostras do passado. O cálculo `readPos = wIdx - tapTime` produz a posição bruta de leitura fracionária.
*   **A Necessidade do If (`readPos < 0.0f`):** Verificou-se que as verificações condicionais e os ajustes somando `DELAY_BUFFER_SIZE` no `dimension_chorus.c` **são essenciais**. Se passarmos um `readPos` negativo (e.g. -2.3) diretamente para o interpolador Hermite, a máscara bit a bit lidaria perfeitamente bem com os índices de array base (-2 vira corretamente o índice do passado da cauda), **porém**, o cálculo da parte fracionária `frac = readPos - (float)i0` falharia matematicamente. Subtrair e forçar o `readPos` para um valor positivo preserva a orientação e extração do polinômio, garantindo que o delay fracionário mantenha integridade. Foi adicionada documentação detalhada sobre isso no próprio código sem alterar o seu comportamento.

## Próximos Passos (Integração STM32)
O core DSP foi comprovado como determinístico e isento de anomalias (NaN/Inf) e boundaries mal calculados nessas verificações iniciais em host. Os próximos passos recomendados seriam:
1.  Avaliar auditivamente os `.wav` criados na pasta `tests/output/` para conferir se o "Dimension Effect" se manifesta sutilmente como esperado (atenção à relação do HPF/LPF na audibilidade da modulação em sweeps e transientes).
2.  Preparar um esqueleto básico para firmware no STM32 CubeIDE:
    *   Configurar a SAI/I2S com DMA Circular e interrupções Half/Full.
    *   Providenciar o driver de CODEC (ex: WM8960 ou similar, a depender da board STM32H5 utilizada).
    *   Instanciar o `DimensionChorusState` e mapear as chamadas do bloco `DimensionChorus_ProcessBlock` para dentro da subrotina de Callback do DMA.

# Projeto de Engenharia DSP: Dimension Chorus para STM32H5

**Plataforma Alvo:** STM32H5 (Cortex-M33 com FPU, ex: H562/H563)
**Formato Numérico:** `float32` nativo.
**Taxa de Amostragem:** 48 kHz (Processamento em blocos via DMA).
**Topologia:** Mono-In / Stereo-Out.

---

## 1. Identidade Sonora do “Dimension Chorus”

O efeito "Dimension" clássico (ex: Boss DC-2) não soa como um chorus tradicional. Ele atua como um "espessador" estéreo, criando uma imagem larga e imersiva sem o balanço enjoativo (wobble) comum em pedais como o CE-2.

**Elementos Essenciais:**
*   **Duas linhas de atraso (vozes) independentes:** Moduladas exatamente em oposição de fase (180°). Quando a linha 1 sobe de pitch, a linha 2 desce.
*   **Modulação sutil (Triangle/Smooth):** Em vez de uma senoide pura, usa-se um triângulo levemente arredondado. Isso faz com que o pitch shift seja constante na maior parte do tempo, mudando suavemente nas pontas.
*   **Preservação do centro:** O dry signal central não recebe modulação. As vozes L e R se equilibram, e a soma das modulações opostas cancela parte do efeito de vibrato no campo estéreo.
*   **Zero Feedback:** Nenhuma realimentação (zero resonance/flanging).
*   **Cross-mixing cuidadoso:** A abertura estéreo vem de injetar o sinal da linha 1 no canal L e o sinal da linha 2 no canal R (e vice-versa invertido em alguns algoritmos).

**O que Evitar:**
*   **LFO Senoidal Rápido ou Profundo:** Gera som de "Leslie" ou CE-2.
*   **Mono Chorus Clássico:** Misturar o Dry com apenas 1 linha modulada gera comb-filtering óbvio (wobble de pitch).
*   **Wet Muito Brilhante:** Um BBD real corta altas frequências (> 6 kHz). Se o wet for digitalmente puro (hi-fi), o efeito soa "metálico" e se descola do sinal seco.
*   **Stereo Widener Genérico:** Atrasos fixos (Haas effect) soam estáticos e causam problemas graves de fase em mono. O Dimension precisa de movimento contínuo nas duas linhas.

---

## 2. Arquitetura DSP Recomendada

A arquitetura para o STM32H5 processa amostras em blocos (ex: 32 amostras por interrupção de DMA), operando em `float32`.

### Diagrama de Blocos Textual
```text
[Input Mono]
   |---> (Dry Gain) -------------------------------------------------> [+] ---> [Out L]
   |                                                                    ^
   |---> [Pre-Emphasis Filter (High Shelf/Bell)]                        |
            |                                                           |
            |---> [BBD Delay Line 1] ---> [Frac Interpolation] ---> [De-Emphasis/LPF 1] ---> [Soft Saturation] ---> (Mix L1) ---> [+] ---> [Out L]
            |                                                                                                  ---> (Mix R1) ---> [+] ---> [Out R]
            |
            |---> [BBD Delay Line 2] ---> [Frac Interpolation] ---> [De-Emphasis/LPF 2] ---> [Soft Saturation] ---> (Mix L2) ---> [+] ---> [Out L]
                                                                                                               ---> (Mix R2) ---> [+] ---> [Out R]

[LFO Core] ---> Generate Triangle (0°)   ---> Smooth LPF ---> Modulate Delay 1
           ---> Generate Triangle (180°) ---> Smooth LPF ---> Modulate Delay 2
```

### Explicação dos Blocos e Equações Simplificadas
*   **Modulação:** As duas linhas de atraso variam em torno de um tempo base.
    *   `Mod1(t) = DelayBase + Depth * LFO_Tri_Smoothed(t)`
    *   `Mod2(t) = DelayBase + Depth * LFO_Tri_Smoothed(t + 180°)`
*   **Matriz Estéreo:** Para espalhar as vozes sem perder o centro, enviamos uma voz mais forte para um lado e invertemos parcialmente no outro (pseudo-M/S).
    *   `Out_L = Dry + (Wet1 * G_wet) - (Wet2 * G_wide)`
    *   `Out_R = Dry + (Wet2 * G_wet) - (Wet1 * G_wide)`
*(O fator G_wide cria uma expansão espacial sem arruinar a compatibilidade mono. Se G_wide for 0, temos um Dual Chorus normal).*

---

## 3. Linhas de Atraso e Modulação

*   **Número de Vozes:** Exatamente **2 vozes**. Adicionar mais vozes consome CPU e dilui o efeito original de "duas fitas" defasadas.
*   **Faixas Plausíveis:**
    *   **Delay Base:** 7.0 ms a 12.0 ms.
    *   **Depth (Profundidade):** 0.5 ms a 3.0 ms (mantido curto para evitar vibrato óbvio).
    *   **Rate (Velocidade):** 0.25 Hz a 1.5 Hz. Acima disso vira Leslie.
*   **Relação de Fase:** Fixada em 180° de defasagem. É o segredo para a modulação de uma voz anular o "balanço" da outra.
*   **Topologia H5:** O caminho **Mono-In / Stereo-Out** é ideal para guitarra. A abordagem M/S interna (usando os ganhos cruzados invertidos na matriz de saída) preserva o grave do instrumento intacto no Dry.

---

## 4. Interpolação Fracionária

Para um Cortex-M33 com FPU processando áudio:
*   **Linear:** Muito barata (1 MAC), mas amortece as altas frequências e introduz ruído de quantização de fase.
*   **Hermite (3ª Ordem / 4 pontos):** **<-- RECOMENDADA (V1)**
    *   **Qualidade:** Excelente preservação do espectro e fase musical. Menos distorção harmônica que a linear.
    *   **Custo Computacional:** Requer ler 4 amostras e calcular polinômios. Custa ~15 ciclos no STM32H5 usando FPU. Perfeitamente viável.
*   **Cúbica (Spline):** Mais cara computacionalmente sem grande vantagem audível sobre Hermite em pedais de guitarra.
*   **All-pass Fracionário:** Útil para preservar espectro, mas dispersa a fase e pode ser instável em modulações rápidas. Não recomendado em C simples.

---

## 5. Caráter Analógico / BBD-like

Para emular os CIs MN3207 (BBD) do Dimension original sem modelagem SPICE:

1.  **Limitação de Banda (Essencial para V1):**
    *   Filtro Pós-Delay (Anti-Aliasing de Clock do BBD original): LPF de 2ª ordem (Biquad) em ~6 kHz a 8 kHz no Wet.
2.  **Saturação Leve (Desejável para V2):**
    *   `Wet = tanh_approx(Wet)` ou aproximação polinomial rápida. O BBD satura rapidamente. Isso "cola" o efeito.
3.  **Pre-emphasis / De-emphasis (Desejável para V2):**
    *   Filtros High-Shelf pré-delay e Low-Shelf pós-delay. Imitam o circuito compander (NE570).
4.  **Ruído / Companding Dinâmico (Desnecessário na V1):**
    *   Complexo e muitos usuários de pedais modernos detestam ruído de clock ("hiss").

---

## 6. LFO e Geração de Modulação

O Dimension requer um "Triângulo Suavizado". Um triângulo perfeito causa um som de "clique" (zipper noise instantâneo de fase) quando a direção do pitch shift inverte abruptamente.

**Decisão H5:**
*   **Phase Accumulator:** Uma variável `phase` (0.0 a 1.0) incrementada a cada amostra (`rate / 48000.0`).
*   **Cálculo Direto:** Transforma a fase em um triângulo (-1.0 a +1.0).
*   **Smoothing LPF (O segredo):** Passar o triângulo por um filtro One-Pole Low-Pass (ex: corte em 2Hz a 5Hz) para arredondar os "bicos" do triângulo.
*   **Atualização:** Atualize o LFO e a leitura fracionária **amostra por amostra**. Atualizar o delay rate apenas por bloco (ex: a cada 32 samples) gera artefatos de modulação terríveis em frequências altas, audíveis como modulação "pixelada" ou zipper noise.

---

## 7. Mixagem e Imagem Estéreo

A matriz estéreo proposta garante amplitude total e compatibilidade mono perfeita.

Equações de matriz:
*   `Out_L = Dry + (Wet1 * MixLevel) - (Wet2 * WideLevel)`
*   `Out_R = Dry + (Wet2 * MixLevel) - (Wet1 * WideLevel)`

**Exemplos de Coeficientes (Modos Fixos):**
*   **Modo 1 (Sutil):** `MixLevel` = 0.50, `WideLevel` = 0.10
*   **Modo 2 (Médio):** `MixLevel` = 0.65, `WideLevel` = 0.20
*   **Modo 3 (Amplo):** `MixLevel` = 0.80, `WideLevel` = 0.35
*   **Modo 4 (Intenso):** `MixLevel` = 1.00, `WideLevel` = 0.50

**Por que a compatibilidade Mono funciona?**
Ao somar L e R (Mono):
`Mono = 2*Dry + (Wet1 + Wet2) * (MixLevel - WideLevel)`
Como Wet1 e Wet2 modulam em 180°, o vibrato de pitch de uma linha cancela o da outra. O resultado é um timbre espesso, sem perda de volume (comb-filtering fixo, mas não oscilante).

---

## 8. Modos e Parâmetros

A UI expõe botões (Modos 1 a 4), mas o motor DSP opera com variáveis contínuas suavizadas.

| Parâmetro Interno | Modo 1 | Modo 2 | Modo 3 | Modo 4 |
| :--- | :--- | :--- | :--- | :--- |
| `Rate (Hz)` | 0.25 | 0.50 | 0.80 | 1.20 |
| `Depth (ms)` | 1.00 | 1.50 | 2.00 | 2.50 |
| `Delay Base (ms)` | 8.00 | 9.00 | 10.00 | 11.00 |
| `Mix (0 a 1)` | 0.50 | 0.65 | 0.80 | 1.00 |
| `Wide (0 a 1)` | 0.10 | 0.20 | 0.35 | 0.50 |

**Proteção (Smoothing):** Quando o usuário troca de Modo, o firmware não atualiza os parâmetros instantaneamente. Ele define os "Targets" (alvos). Em "control-rate" (início de cada bloco DMA), os valores atuais movem-se gradualmente para os alvos:
`currentRate += 0.05f * (targetRate - currentRate);`
Isso evita estalos ao trocar de preset.

---

## 9. Headroom, Ganho e Não Linearidade

*   **Entrada:** Assuma o sinal em `float32` variando entre -1.0 e +1.0.
*   **Soma Dry+Wet:** A soma de sinais pode exceder 1.0. O motor H5 FPU não clipa internamente.
*   **Limitador Final:** Antes de converter de `float32` para inteiros do DMA I2S (ex: Q31), passe a saída L e R por uma saturação polinomial segura ou Soft Clipper para proteger contra wrap-around (quando o sinal excede a escala e inverte o sinal no DAC digital):
    `out = x * (1.5f - 0.5f * x * x);` (aplicado apenas se `|x| > threshold`).

---

## 10. Taxa de Amostragem e Tamanho de Bloco

*   **Taxa:** 48 kHz.
    *   **Vantagem:** Qualidade transparente. Filtros do BBD (6kHz) têm resposta perfeita a 48kHz sem warping perceptível.
*   **Tamanho do Bloco (DMA):** 32 amostras por canal.
    *   Em I2S Stereo (32-bit words), a interrupção de *Half-Transfer* processa 32 frames e a *Transfer-Complete* processa os outros 32.
    *   Isso dá uma latência de processamento de `32 / 48000 = ~0.66 ms`, totalmente imperceptível para um guitarrista, e amortece bem o overhead de chamada de funções.

---

## 11. Float vs Fixed no STM32H5

**Estratégia Escolhida: `float32` nativo.**
*   O Cortex-M33 H5 tem unidade de ponto flutuante (FPU).
*   Instruções de soma e multiplicação levam ciclos semelhantes a inteiros.
*   A interpolação Hermite e os coeficientes IIR do Biquad ficam consideravelmente mais simples de escrever e manter em float. Em Q31, teríamos risco de overflow intermediário nas frações da interpolação, exigindo shifts que gastam mais CPU.

---

## 12. Uso de Memória no STM32H5

O MCU H563 possui > 600KB de SRAM. Há imensa folga para o chorus.
*   **Delay Buffer (RAM Interna):** Buffer circular. Max Delay de 15 ms = 720 amostras @ 48kHz.
    *   Alocar como potência de 2 para otimização do índice (Bitwise AND): **1024 amostras**.
    *   2 vozes * 1024 amostras * 4 bytes = **8 KB de RAM**.
*   **Memória DMA:** Buffer circular I2S ping-pong = ~512 Bytes. Deve ser posicionado em área *Non-Cacheable* configurada pela MPU, ou gerenciar limpeza manual com `SCB_CleanInvalidateDCache()`.
*   **Workspace (Estados):** Filtros e variáveis: < 200 Bytes.

---

## 13. Orçamento de CPU

Processador STM32H5 a ~250 MHz.
Para processar 1 amostra estéreo @ 48kHz:
1. Geração LFOs (Tri + OnePole LPF): ~15 ciclos.
2. Interpolação Hermite (2x): ~40 ciclos.
3. Filtros Biquad (2x): ~20 ciclos.
4. Matriz de Mix + Soft Clip: ~15 ciclos.
**Total de CPU no hot-path:** < 100 ciclos por amostra.
`100 ciclos * 48000 Hz = 4.8 MHz.`
Isso corresponde a cerca de **2% do processador**. Há sobra generosa para UI, expansões e processamento de blocos DMA.

---

## 14. Arquitetura de Firmware

```c
typedef struct {
    float delayBuffer[2][1024];
    uint32_t writeIndex;

    // Parâmetros (Alvo e Atual para Smoothing)
    float targetRate, targetDepth, targetMix, targetWide;
    float rate, depth, mix, wide; // current

    // LFO State
    float phase;
    float lfoSmoothed[2];

    // Filtros Wet (estados Biquad LPF 6kHz)
    float filterL[4];
    float filterR[4];
} DimensionChorusState;
```
1. O Hardware DMA levanta a interrupção I2S.
2. A interrupção seta uma `flag_process_audio_half` ou `flag_process_audio_full`.
3. O Superloop Bare-Metal (Main) detecta a flag.
4. Aplica Smoothing nas variáveis de controle (`current += coef * (target - current)`).
5. Chama `ProcessChorusBlock(state, input_buffer, output_buffer, 32)`.
6. Limpa a flag.

---

## 15. Pseudocódigo em Estilo C/C++

*Hot-path do processo de um bloco de 32 amostras em float32.*

```c
void ProcessChorusBlock(DimensionChorusState* state, const float* inMono, float* outStereo, size_t blockSize) {
    for (size_t i = 0; i < blockSize; i++) {
        float dry = inMono[i];

        // 1. Escrita nos Buffers de Delay
        state->delayBuffer[0][state->writeIndex] = dry;
        state->delayBuffer[1][state->writeIndex] = dry;

        // 2. LFO (Phase Accumulator & Smoothed Triangle)
        state->phase += state->rate / 48000.0f;
        if (state->phase >= 1.0f) state->phase -= 1.0f;

        // Triângulo de Fase 0 e 180
        float tri0 = (state->phase < 0.5f) ? (4.0f * state->phase - 1.0f) : (3.0f - 4.0f * state->phase);
        float tri180 = -tri0;

        // Filtro One-Pole LPF no LFO (Arredonda os bicos)
        state->lfoSmoothed[0] += 0.05f * (tri0 - state->lfoSmoothed[0]);
        state->lfoSmoothed[1] += 0.05f * (tri180 - state->lfoSmoothed[1]);

        // 3. Cálculo de offsets em amostras (Modulação)
        // Delay Base fixado em 9ms para simplificar (9ms * 48 samples/ms = 432 amostras)
        float baseSamps = 432.0f;
        float depthSamps = state->depth * 48.0f;

        float dTime0 = baseSamps + depthSamps * state->lfoSmoothed[0];
        float dTime1 = baseSamps + depthSamps * state->lfoSmoothed[1];

        // 4. Leitura Interpolação Hermite
        float wet1 = ReadHermite(state->delayBuffer[0], state->writeIndex, dTime0);
        float wet2 = ReadHermite(state->delayBuffer[1], state->writeIndex, dTime1);

        // 5. Filtros BBD (Anti-Aliasing Biquad)
        wet1 = ProcessBiquad(wet1, state->filterL); // Presume LPF 6kHz
        wet2 = ProcessBiquad(wet2, state->filterR);

        // 6. Matriz Dimension
        float outL = dry + (wet1 * state->mix) - (wet2 * state->wide);
        float outR = dry + (wet2 * state->mix) - (wet1 * state->wide);

        // 7. Output Soft Clipper e Store Intercalado (I2S Stereo)
        outStereo[i*2]   = SoftClip(outL);
        outStereo[i*2+1] = SoftClip(outR);

        // 8. Avança índice (Buffer de 1024)
        state->writeIndex = (state->writeIndex + 1) & 1023;
    }
}
```

---

## 16. Estratégia de Tuning

1.  **Ouça apenas 1 Wet Line:** Comece sem matriz estéreo. Afine a modulação ajustando o coeficiente de suavização do LFO (`0.05f`). O pitch não pode "estalar", deve deslizar suavemente.
2.  **Calibre a Largura:** Com as duas vozes ligadas em L e R, aumente o `Wide` (Wide Mix). A imagem deve abrir para as bordas dos alto-falantes. Pare antes que pareça "fora de fase" no centro.
3.  **Filtragem "Analógica":** Ajuste o cutoff do LPF Biquad entre 4kHz e 8kHz. Quando você palhetar forte na guitarra, o ataque deve soar nítido do sinal Dry, mas a cauda da modulação deve ser quente e escura (BBD). Se o wet estiver agudo demais, vira um chorus de plugin dos anos 2000.
4.  **Teste de Mono:** Use uma mixadeira e some L+R. O volume não deve cair consideravelmente e a oscilação de pitch deve diminuir (cancelamento dos 180°), mas a textura deve continuar rica.

---

## 17. Estratégia por Versões (Roadmap)

*   **V1 (Mínima Viável - Esta Arquitetura):**
    *   **Inclui:** Motor Float32 a 48kHz, Mono-In/Stereo-Out, LFO Triângulo com Smoothing, Hermite, LPF simples no Wet, Matriz Pseudo M/S.
    *   **Fica de fora:** Saturação de fita, Pre/De-emphasis em alta fidelidade.
    *   **Ideal para:** Validar o caráter sônico. Consome míseros ~2% de CPU H5.
*   **V2 (Refinada):**
    *   **Inclui:** Saturação polinomial dentro do path do Wet para simular headroom reduzido de MN3207. Filtros pré e pós para emular Companding de EQ. UI com parâmetros continuamente variáveis nos potenciômetros.
    *   **Custo:** Sobe para ~4% da CPU. Traz vibração harmônica rica nas palhetadas mais fortes.
*   **V3 (Premium):**
    *   **Inclui:** True Stereo In (requer L e R somados no centro ou 4 linhas de modulação independentes). Capacidade MIDI e mapeamento de footswitch via RTOS.

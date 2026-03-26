# Projeto de Engenharia DSP: Dimension Chorus para STM32H5

**Plataforma Alvo:** STM32H5 (Cortex-M33 com FPU, ex: H562/H563)
**Formato Numérico:** `float32` nativo.
**Taxa de Amostragem:** 48 kHz (Processamento em blocos via DMA).
**Topologia:** Mono-In / Stereo-Out.

---

## 1. Identidade Sonora do “Dimension Chorus”

O efeito "Dimension" clássico (ex: Boss DC-2) atua como um "espessador" estéreo, criando uma imagem larga e imersiva sem o balanço enjoativo (wobble) de chorus tradicionais (como o CE-2).

**Elementos Essenciais:**
*   **Duas vozes modulares sobrepostas:** Moduladas exatamente em oposição de fase (180°). Quando a voz 1 sobe de pitch, a voz 2 desce.
*   **Modulação sutil e alongada:** Utiliza-se um triângulo com waveshaping/filtragem suave para que a mudança de pitch deslize, e o *depth* é muito conservador.
*   **Preservação do centro e ausência de feedback:** O *dry* domina os transientes centrais. Nenhuma realimentação (resonance/flanging) é aplicada no *wet*.
*   **Voicing analógico escuro:** O *wet* possui menos agudos e graves que o sinal original (imitando BBDs e as características de banda limitada).
*   **Cross-mixing estéreo cuidadoso:** A abertura espacial não vem de panning estrito, mas de uma matriz que envia a voz principal para um lado e uma inversão parcial para o outro.

---

## 2. Arquitetura DSP Recomendada (v1 Mínima Viável)

A arquitetura processa blocos em `float32` via DMA (ex: 32 frames por interrupção).

### Diagrama de Blocos Textual
```text
[Input Mono]
   |---> (Dry Gain) -----------------------------------------------------------------------> [+] ---> [Out L]
   |                                                                                          ^
   |---> [Single Delay Buffer (Circular)]                                                     |
            |                                                                                 |
            |---> [Tap 1 / Frac Hermite] ---> [HPF + LPF] ---> [Trim] ---> (Mix L1) --------> [+] ---> [Out L]
            |                                                         ---> (Mix R1) --------> [+] ---> [Out R]
            |
            |---> [Tap 2 / Frac Hermite] ---> [HPF + LPF] ---> [Trim] ---> (Mix L2) --------> [+] ---> [Out L]
                                                                      ---> (Mix R2) --------> [+] ---> [Out R]

[LFO Core] ---> LFO Triângulo com Filtragem a = 1 - exp(-2π*fc/fs)
           ---> Modulate Tap 1 (0°)
           ---> Modulate Tap 2 (180°)
```

### Explicação dos Blocos
*   **Buffer Único:** Economia de recursos. O *dry* é escrito em um único array circular mono. Lemos os tempos fracionários em duas posições de atraso dinâmicas distintas.
*   **Modulação Conservadora:**
    *   `DelayTap1 = BaseDelay + Depth * LFO(t)`
    *   `DelayTap2 = BaseDelay + Depth * (-LFO(t))`
*   **Matriz Estéreo Ajustada:** Projetada não apenas para simetria visual, mas para manter o somMono "gordo" e estável, reduzindo cancelamentos de fase problemáticos.
    *   `Out_L = (Dry_Gain * Dry) + (Main_Wet * Tap1) - (Cross_Wet * Tap2)`
    *   `Out_R = (Dry_Gain * Dry) + (Main_Wet * Tap2) - (Cross_Wet * Tap1)`

---

## 3. Linhas de Atraso e Modulação

*   **Número de Vozes:** 2 taps fracionários sobre um buffer único.
*   **Faixas de Calibração (Foco em Dimension):**
    *   **Base Delay:** 8.0 ms a 16.0 ms. Valores ligeiramente mais longos ajudam a afastar do comb-filter apertado.
    *   **Depth Efetivo:** ±0.6 ms a ±1.8 ms. Muito mais conservador que um chorus comum.
    *   **Rate:** 0.15 Hz a 0.85 Hz. Acima de 1Hz desvia da identidade sonora de "largura espacial".

---

## 4. Interpolação Fracionária

*   **Recomendação STM32H5:** **Hermite (3ª Ordem / 4 pontos)** em `float32`.
    *   Equilíbrio ideal entre custo (cerca de 15 ciclos FPU por tap), previsibilidade e manutenção da alta fidelidade do topo espectral comparado à interpolação linear (que "embota" os agudos nas áreas não-inteiras e gera *zipper noise* em modulação de pitch lenta).

---

## 5. Caráter Analógico / Voicing do Wet

O caráter *Dimension* exige que as vozes modulares não sejam "puristas" (Hi-Fi).
1.  **HPF Leve (High-Pass, ~120 Hz):** Retira o *mud* do *wet*. Mantém os graves e o impacto centrados firmemente no sinal *dry*, enquanto a modulação abre apenas na banda média e média-alta.
2.  **LPF BBD (Low-Pass, ~5.5 kHz a 7 kHz):** Esconde a precisão digital da interpolação e imita a perda de inserção analógica do circuito BBD + Aliasing Filter original.

---

## 6. LFO e Geração de Modulação

*   **Smoothing do LFO (Forma da Onda):**
    Usamos um LFO triangular via acumulador de fase, passado por um filtro RC-Digital (One-Pole) cujo coeficiente deve ser definido explicitamente de acordo com a taxa de amostragem:
    *   `a_lfo = 1.0f - exp(-2 * PI * fc_LFO / fs)`
    *   Isso garante consistência e arredonda apenas os picos de inversão (onde o triângulo cria o desagradável *click* de aceleração súbita).
*   **Smoothing de Parâmetros de Interface (UI):**
    Separado do LFO. `TargetRate`, `TargetDepth`, etc., são suavizados em controle de bloco (Control-Rate) para que o giro do footswitch/preset não crie *zipper noise* nas variáveis mestras.

---

## 7. Mixagem, Headroom e Imagem Estéreo

A matriz estéreo L/R deve entregar uma boa experiência tanto em fones de ouvido quanto somada no PA (Mono Mix). Em mono, as variações de pitch de cada voz diminuem, resultando em um timbre quente sem pulsação de vibrato.

**Proteção de Headroom por Ganho (Arquitetura de Segurança):**
Em vez de depender do clipe final, configuramos ativamente a matemática:
*   Reduzimos o *dry* fixo: `Dry_Gain = 0.88f a 0.94f`
*   As amplitudes de `Main_Wet` e `Cross_Wet` preenchem o mix estéreo sem que os transientes ultrapassem facilmente o 1.0f do barramento `float32`.
*   O **Soft Clip Final** serve **apenas** para segurar os picos dinâmicos que ainda assim passarem (evitando wrap-around de DAC).

---

## 8. Modos Iniciais Recomendados

A v1 deve ter o comportamento guiado por 4 modos pré-calibrados, simulando os botões do original:

| Modo | Rate (Hz) | Base Delay (ms) | Depth (ms) | Main Wet | Cross Wet |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | 0.18 | 10.5 | 0.70 | 0.20 | 0.06 |
| **2** | 0.32 | 11.5 | 1.00 | 0.26 | 0.09 |
| **3** | 0.55 | 12.5 | 1.30 | 0.32 | 0.12 |
| **4** | 0.82 | 13.0 | 1.60 | 0.38 | 0.16 |

*O `Dry_Gain` fica travado em `0.88`, permitindo bom headroom e impacto central.*

---

## 9. Taxa de Amostragem, Tamanho de Bloco e Execução

*   **Taxa e Formato:** 48 kHz / `float32`. A FPU garante ausência de perda de precisão em baixas modulações (problema comum no Q31) sem custo aparente em ciclos.
*   **Blocos DMA:** 32 amostras por interrupção.
*   **Contrato Temporal (Firmware):** O Superloop/Callback processa os flags do DMA imediatamente após a ISR levantar a interrupção. A arquitetura exige que o *main loop* seja cooperativo, mantendo o controle de UI (encoders/LEDs) o mais curto possível para nunca bloquear a janela do bloco de áudio (latency < 0.66 ms).

---

## 10. Uso de Memória e Custo de CPU no STM32H5

*   **Delay Buffer:** Um array circular mono.
    *   Sendo `16 ms` o delay máximo = 768 amostras @ 48kHz.
    *   Potência de 2 para o wrap circular: **1024 amostras**. `1024 * 4 bytes = 4 KB` de RAM interna.
*   **Workspace (Estados):** Filtros e variáveis pesam < 200 Bytes.
*   **Custo Computacional:** Muito leve a leve. Espera-se que o motor hot-path demande substancialmente menos de 10% da CPU do H563 a 250 MHz, deixando vasta margem (folga confortável) para conectividade e drivers adicionais.

---

## 11. Pseudocódigo em Estilo C/C++ (V1)

```c
typedef struct {
    float delayBuffer[1024]; // 1 Buffer Mono
    uint32_t writeIndex;

    // Parâmetros de UI / Control
    float targetRate, targetBaseMs, targetDepth, targetMainW, targetCrossW;
    float rate, baseMs, depth, mainW, crossW; // Smoothed states

    // LFO State
    float phaseAcc;
    float lfoSmoothed;
    float lfoCoef; // Pre-calculado: 1.0f - exp(-2*PI * fc / 48000)

    // Filtros do Wet (Voicing)
    float wetFilter1_HPF[4];
    float wetFilter1_LPF[4];
    float wetFilter2_HPF[4];
    float wetFilter2_LPF[4];

} DimensionChorusState;

// ...

void ProcessChorusBlock(DimensionChorusState* state, const float* inMono, float* outStereo, size_t blockSize) {
    const float dryGain = 0.88f;

    for (size_t i = 0; i < blockSize; i++) {
        float dry = inMono[i];

        // 1. Snapshot do index local e escrita no buffer Mono
        uint32_t wIdx = state->writeIndex;
        state->delayBuffer[wIdx] = dry;

        // 2. Geração do LFO com coef formal
        state->phaseAcc += state->rate / 48000.0f;
        if (state->phaseAcc >= 1.0f) state->phaseAcc -= 1.0f;

        float triRaw = (state->phaseAcc < 0.5f) ? (4.0f * state->phaseAcc - 1.0f) : (3.0f - 4.0f * state->phaseAcc);

        // Smoothing exclusivo da forma do LFO
        state->lfoSmoothed += state->lfoCoef * (triRaw - state->lfoSmoothed);

        // 3. Leituras moduladas (Delay Base e Depth convertidos em amostras)
        float baseSamps = state->baseMs * 48.0f;
        float depthSamps = state->depth * 48.0f;

        float tapTime1 = baseSamps + (depthSamps * state->lfoSmoothed);
        float tapTime2 = baseSamps - (depthSamps * state->lfoSmoothed); // 180° = inversão

        // 4. Interpolação Hermite 3ª Ordem
        float wet1 = ReadHermite(state->delayBuffer, wIdx, tapTime1);
        float wet2 = ReadHermite(state->delayBuffer, wIdx, tapTime2);

        // 5. Voicing Analógico (HPF p/ limpar grave, LPF p/ limpar brilho BBD)
        wet1 = ProcessBiquad(ProcessBiquad(wet1, state->wetFilter1_HPF), state->wetFilter1_LPF);
        wet2 = ProcessBiquad(ProcessBiquad(wet2, state->wetFilter2_HPF), state->wetFilter2_LPF);

        // 6. Matriz Estéreo e Headroom
        float outL = (dryGain * dry) + (wet1 * state->mainW) - (wet2 * state->crossW);
        float outR = (dryGain * dry) + (wet2 * state->mainW) - (wet1 * state->crossW);

        // 7. Output Trim + Soft Clip final e formatação I2S
        outStereo[i*2]   = SoftClip(outL);
        outStereo[i*2+1] = SoftClip(outR);

        // Avança Buffer
        state->writeIndex = (wIdx + 1) & 1023;
    }
}
```

---

## 12. Estratégia de Tuning Adicional (Pós-Implementação)

1.  **Refino de Matriz:** Brinque com os coeficientes da tabela de 4 modos. A matemática teórica da matriz pseudo-M/S precisa ser avaliada ouvindo em caixas, em *headphones* e somando em mono físico para validar se o cancelamento central é satisfatório.
2.  **Calibração do LFO (`fc_LFO`):** O parâmetro `fc` da equação $1 - e^{-2\pi f_c/f_s}$ deve estar em torno de **1 a 4 Hz**. Frequência muito baixa = lfo quadrado. Muito alta = perde-se a correção nos picos do triângulo.
3.  **Soft-Clip Analógico:** Adicione overdrive polinomial dentro do próprio canal do wet (antes dos filtros LPF) na versão v2.

# Especificação Técnica v1.0  
## Dimension Chorus para STM32H5

**Plataforma alvo:** STM32H5 (Cortex-M33 com FPU, ex.: STM32H562 / STM32H563)  
**Formato numérico:** `float32` nativo  
**Taxa de amostragem:** 48 kHz  
**Topologia principal:** Mono-In / Stereo-Out  
**Modelo de execução:** processamento em blocos via DMA (`half-transfer` / `transfer-complete`)  
**Objetivo da v1:** obter um chorus do tipo *Dimension* com identidade sonora convincente, baixo risco de artefatos, baixo custo computacional e implementação robusta em firmware embarcado.

---

## 1. Objetivo sonoro

O efeito deve reproduzir a estética de um **Dimension Chorus**: ampliar a imagem estéreo e engrossar o timbre sem soar como um chorus tradicional de modulação evidente.

### Características desejadas
- **Imagem estéreo larga e estável**
- **Modulação discreta**, sem sensação forte de “wobble”
- **Centro preservado** pelo domínio do sinal dry
- **Ausência de feedback**
- **Wet com voicing escurecido**, lembrando uma linha analógica limitada em banda
- **Boa compatibilidade mono**, com redução perceptual da modulação cíclica quando L+R são somados

### Características a evitar
- profundidade excessiva de modulação
- LFO rápido demais
- wet brilhante demais
- comb filtering muito evidente
- comportamento de flanger
- sensação de stereo widener estático em vez de chorus espacial

---

## 2. Arquitetura DSP da v1

A implementação v1 usa **um único buffer circular mono** e **duas leituras fracionárias moduladas** sobre esse buffer.

### Diagrama de blocos textual

```text
[Input Mono]
   |---> (Dry Gain) ---------------------------------------------------------------> [+] ---> Out L
   |                                                                                    ^
   |---> [Single Circular Delay Buffer]                                                 |
            |                                                                           |
            |---> [Frac Tap 1 / Hermite] ---> [Wet HPF] ---> [Wet LPF] ---> [Wet Trim] ---> (L1) ---> [+] ---> Out L
            |                                                                             \-> (R1) ---> [+] ---> Out R
            |
            |---> [Frac Tap 2 / Hermite] ---> [Wet HPF] ---> [Wet LPF] ---> [Wet Trim] ---> (L2) ---> [+] ---> Out L
                                                                                          \-> (R2) ---> [+] ---> Out R

[LFO Core]
   └──> phase accumulator
   └──> triangle generation
   └──> one-pole smoothing
   └──> Tap 1 = Base + Depth * LFO
   └──> Tap 2 = Base - Depth * LFO

[Output]
   └──> output trim
   └──> safety soft clip
```

---

## 3. Estrutura matemática

### 3.1 Delay modulado

Para cada amostra:

\[
d_1[n] = D_{base} + D_{depth}\cdot lfo[n]
\]

\[
d_2[n] = D_{base} - D_{depth}\cdot lfo[n]
\]

onde:
- \(D_{base}\) é o atraso base em amostras
- \(D_{depth}\) é a excursão da modulação em amostras
- \(lfo[n]\) está aproximadamente em \([-1, +1]\)

### 3.2 Leitura das vozes

\[
w_1[n] = \text{FracRead}(buffer, writeIndex, d_1[n])
\]

\[
w_2[n] = \text{FracRead}(buffer, writeIndex, d_2[n])
\]

A função `FracRead` deve usar **interpolação Hermite de 3ª ordem / 4 pontos**.

### 3.3 Voicing do wet

Cada voz é filtrada:

\[
\tilde{w}_1[n] = LPF(HPF(w_1[n]))
\]

\[
\tilde{w}_2[n] = LPF(HPF(w_2[n]))
\]

### 3.4 Matriz estéreo

\[
y_L[n] = G_{dry}\cdot x[n] + G_{main}\cdot \tilde{w}_1[n] - G_{cross}\cdot \tilde{w}_2[n]
\]

\[
y_R[n] = G_{dry}\cdot x[n] + G_{main}\cdot \tilde{w}_2[n] - G_{cross}\cdot \tilde{w}_1[n]
\]

Essa matriz deve ser entendida como **ponto de partida de voicing**, não como identidade matemática absoluta. Os ganhos podem ser refinados por audição.

---

## 4. Escolhas técnicas da v1

### 4.1 Número de vozes
A v1 usa **2 vozes moduladas**.  
Isso é suficiente para obter o comportamento desejado e evita:
- diluição da identidade do efeito
- aumento desnecessário de CPU
- maior complexidade de tuning

### 4.2 Buffer de delay
Usar **1 buffer circular mono**.

Vantagens:
- menor uso de RAM
- menor tráfego de memória
- implementação mais simples
- duas vozes a partir de dois taps fracionários independentes

### 4.3 Feedback
A v1 usa **feedback = 0**.

### 4.4 Topologia de I/O
A v1 é definida como:
- **entrada mono**
- **saída estéreo**

Entrada estéreo, mid/side ou true stereo ficam fora da v1.

---

## 5. Faixas de projeto

### 5.1 Delay base
Faixa operacional recomendada:
- **8,0 ms a 16,0 ms**

Faixa típica da v1:
- **10,5 ms a 13,0 ms**

### 5.2 Depth
Faixa operacional recomendada:
- **±0,6 ms a ±1,8 ms**

Na v1, o depth deve ser **conservador**.

### 5.3 Rate
Faixa operacional recomendada:
- **0,15 Hz a 0,85 Hz**

Acima de ~1 Hz o efeito tende a perder o caráter “dimension”.

---

## 6. Interpolação fracionária

### Escolha da v1
**Hermite 3ª ordem / 4 pontos**

### Justificativa
- melhor qualidade que interpolação linear
- menor perda de brilho nas leituras fracionárias
- menor sensação de granulação em modulação lenta
- custo ainda baixo para STM32H5 em `float32`
- implementação simples e estável em C

### Opções descartadas para a v1
- **Linear:** barata, mas tende a soar mais áspera e mais “digital”
- **All-pass fracionário:** mais delicada e menos direta para este caso
- **Splines mais pesadas:** ganho pequeno frente ao custo/complexidade

---

## 7. LFO e modulação

### 7.1 Estrutura
A v1 usa:
- `phase accumulator`
- geração de triângulo
- suavização por filtro one-pole

### 7.2 Forma de onda
O LFO não deve ser triângulo “duro”.  
Deve ser **triângulo suavizado**, para reduzir a brusquidão na inversão da rampa.

### 7.3 Equação do smoothing
O coeficiente deve ser calculado explicitamente:

\[
a_{lfo} = 1 - e^{-2\pi f_c / f_s}
\]

onde:
- \(f_c\) é a frequência de suavização do LFO
- \(f_s = 48000\) Hz

### 7.4 Faixa sugerida para \(f_c\)
- **1 Hz a 4 Hz**
- ponto inicial recomendado: **2 Hz**

### 7.5 Segunda voz
Na v1:
- a voz 2 é gerada por oposição nominal de fase:

\[
tap_2 = base - depth \cdot lfo
\]

No futuro isso pode ser refinado com pequenas assimetrias, mas não faz parte da v1.

---

## 8. Voicing analógico do wet

A assinatura sonora depende fortemente de o wet não soar “limpo demais”.

### 8.1 HPF do wet
Função:
- retirar grave excessivo da modulação
- manter o centro e o peso principal no dry

Faixa recomendada:
- **80 Hz a 150 Hz**

Valor inicial sugerido:
- **120 Hz**

### 8.2 LPF do wet
Função:
- escurecer o wet
- esconder o brilho digital da interpolação
- aproximar o caráter de uma linha analógica limitada

Faixa recomendada:
- **5,0 kHz a 7,0 kHz**

Valor inicial sugerido:
- **6,2 kHz**

### 8.3 Estrutura dos filtros
Para a v1:
- HPF e LPF simples por voz
- implementação IIR tipo biquad
- coeficientes recalculados apenas quando necessário, não por amostra

### 8.4 Recursos fora da v1
Ficam para versões futuras:
- companding
- saturação interna mais complexa
- pre-emphasis / de-emphasis completos
- ruído intencional
- modelagem detalhada de BBD

---

## 9. Ganho, headroom e proteção

### 9.1 Princípio
A v1 não deve depender do clipper para funcionar normalmente.  
O clipper é apenas rede de proteção.

### 9.2 Dry gain
Faixa recomendada:
- **0,88 a 0,94**

Valor inicial da v1:
- **0,88**

### 9.3 Wet gains
Os ganhos `Main Wet` e `Cross Wet` devem ser escolhidos de modo que:
- o centro permaneça estável
- a imagem abra com naturalidade
- o barramento interno mantenha folga

### 9.4 Soft clip final
Deve existir um **soft clip final leve** para segurar picos ocasionais antes da conversão para o formato de saída do codec/DAC.

Esse bloco:
- não deve ser o mecanismo principal de voicing
- não deve atuar de forma audível na operação normal
- deve ficar perto do fim da cadeia

---

## 10. Modos da v1

A interface externa da v1 usa **4 modos fixos**, inspirados na filosofia do original.

### Tabela inicial de modos

| Modo | Rate (Hz) | Base Delay (ms) | Depth (ms) | Main Wet | Cross Wet |
|---|---:|---:|---:|---:|---:|
| 1 | 0,18 | 10,5 | 0,70 | 0,20 | 0,06 |
| 2 | 0,32 | 11,5 | 1,00 | 0,26 | 0,09 |
| 3 | 0,55 | 12,5 | 1,30 | 0,32 | 0,12 |
| 4 | 0,72 | 13,0 | 1,45 | 0,36 | 0,15 |

### Observações
- esses valores são **pontos iniciais de voicing**, não calibração final definitiva
- o modo 4 permanece propositalmente contido para evitar virar chorus tradicional
- o `Dry Gain` permanece fixo na v1

---

## 11. Smoothing de parâmetros

O smoothing de parâmetros de UI deve ser **separado** do smoothing do LFO.

### Parâmetros suavizados
- `rate`
- `baseMs`
- `depth`
- `mainW`
- `crossW`

### Política recomendada
- smoothing em **control-rate**, uma vez por bloco de áudio
- atualização usando abordagem exponencial simples

Exemplo:

```c
current += k * (target - current);
```

### Observação
O valor de `k` deve ser escolhido em função do tempo desejado de transição.  
Valores típicos darão transições na ordem de dezenas de milissegundos.

---

## 12. Arquitetura de firmware

### 12.1 Modelo de execução
- periférico de áudio via SAI/I2S
- DMA circular
- callbacks de `half-transfer` e `transfer-complete`
- processamento em blocos curtos

### 12.2 Política de software
A arquitetura-base assume:
- **bare-metal**
- main loop cooperativo
- nenhuma operação bloqueante competindo com o áudio

### 12.3 Regras
- ISR de DMA deve ser curta
- ISR apenas sinaliza qual metade do buffer deve ser processada
- processamento do bloco ocorre fora da ISR, em contexto previsível
- UI, LEDs, leitura de controles, armazenamento e comunicação devem ser não bloqueantes

### 12.4 Tamanho de bloco
Valor recomendado:
- **32 frames por bloco**

Latência de bloco:

\[
32 / 48000 \approx 0,67 \text{ ms}
\]

Isso é adequado para pedal de instrumento.

---

## 13. Estrutura de memória

### 13.1 Delay buffer
- 1 buffer mono
- tamanho fixo: **1024 amostras**
- formato: `float`
- consumo: **4096 bytes**

### 13.2 Buffers DMA
Dependem do formato do periférico, mas devem ficar em região adequada para DMA.

### 13.3 Estados do DSP
Incluem:
- índices
- estado do LFO
- estados de filtros
- parâmetros current/target

Consumo total pequeno comparado à SRAM disponível.

### 13.4 Política de memória
- hot path somente em **SRAM interna**
- evitar memória externa
- organizar buffers de DMA conforme exigências de coerência/cache da plataforma

---

## 14. Custo computacional

A v1 deve ser classificada como:

- **muito leve a leve** para STM32H5
- confortavelmente abaixo do orçamento total do processador

Os blocos que mais pesam são:
- interpolação Hermite
- filtros do wet

Mesmo assim, a carga total esperada permanece baixa o suficiente para coexistir com:
- UI
- LEDs
- leitura de controles
- lógica de preset
- periféricos auxiliares

Sem prometer número exato de ciclos, a expectativa é de **folga ampla** no STM32H563.

---

## 15. Estruturas sugeridas em C

```c
typedef struct {
    float delayBuffer[1024];
    uint32_t writeIndex;

    // Targets de controle
    float targetRate;
    float targetBaseMs;
    float targetDepth;
    float targetMainW;
    float targetCrossW;

    // Parâmetros suavizados
    float rate;
    float baseMs;
    float depth;
    float mainW;
    float crossW;

    // Estado do LFO
    float phaseAcc;
    float lfoSmoothed;
    float lfoCoef;

    // Estados dos filtros das duas vozes
    float wet1HpfState[4];
    float wet1LpfState[4];
    float wet2HpfState[4];
    float wet2LpfState[4];

} DimensionChorusState;
```

---

## 16. Pseudocódigo de controle

### Atualização dos parâmetros por bloco

```c
static inline void Dimension_UpdateControl(DimensionChorusState* s)
{
    const float k = 0.08f;

    s->rate   += k * (s->targetRate   - s->rate);
    s->baseMs += k * (s->targetBaseMs - s->baseMs);
    s->depth  += k * (s->targetDepth  - s->depth);
    s->mainW  += k * (s->targetMainW  - s->mainW);
    s->crossW += k * (s->targetCrossW - s->crossW);
}
```

---

## 17. Pseudocódigo do núcleo DSP

```c
void ProcessChorusBlock(
    DimensionChorusState* state,
    const float* inMono,
    float* outStereo,
    size_t blockSize)
{
    const float dryGain = 0.88f;

    Dimension_UpdateControl(state);

    const float baseSamps  = state->baseMs * 48.0f;
    const float depthSamps = state->depth  * 48.0f;

    uint32_t wIdx = state->writeIndex;

    for (size_t i = 0; i < blockSize; i++) {
        const float dry = inMono[i];

        // Escreve entrada no buffer circular
        state->delayBuffer[wIdx] = dry;

        // LFO
        state->phaseAcc += state->rate / 48000.0f;
        if (state->phaseAcc >= 1.0f) {
            state->phaseAcc -= 1.0f;
        }

        float triRaw;
        if (state->phaseAcc < 0.5f) {
            triRaw = 4.0f * state->phaseAcc - 1.0f;
        } else {
            triRaw = 3.0f - 4.0f * state->phaseAcc;
        }

        state->lfoSmoothed += state->lfoCoef * (triRaw - state->lfoSmoothed);

        // Tempos modulados
        const float tapTime1 = baseSamps + depthSamps * state->lfoSmoothed;
        const float tapTime2 = baseSamps - depthSamps * state->lfoSmoothed;

        // Leitura fracionária
        float wet1 = ReadHermite(state->delayBuffer, wIdx, tapTime1);
        float wet2 = ReadHermite(state->delayBuffer, wIdx, tapTime2);

        // Voicing do wet
        wet1 = ProcessBiquad(wet1, state->wet1HpfState);
        wet1 = ProcessBiquad(wet1, state->wet1LpfState);

        wet2 = ProcessBiquad(wet2, state->wet2HpfState);
        wet2 = ProcessBiquad(wet2, state->wet2LpfState);

        // Matriz estéreo
        float outL = dryGain * dry + state->mainW * wet1 - state->crossW * wet2;
        float outR = dryGain * dry + state->mainW * wet2 - state->crossW * wet1;

        // Proteção final
        outStereo[2 * i + 0] = SoftClip(outL);
        outStereo[2 * i + 1] = SoftClip(outR);

        // Incremento circular
        wIdx = (wIdx + 1u) & 1023u;
    }

    state->writeIndex = wIdx;
}
```

---

## 18. Inicialização recomendada

Na inicialização do efeito, devem ser feitos:

- zerar buffer de delay
- zerar estados dos filtros
- zerar fase e estado do LFO
- configurar o modo inicial
- copiar os targets iniciais para os parâmetros correntes
- calcular `lfoCoef`
- calcular coeficientes dos filtros HPF/LPF

### Regra importante
Coeficientes de filtro:
- recalcular **somente ao trocar modo** ou ao alterar configuração relevante
- não recalcular por amostra

---

## 19. Estratégia de tuning

### 19.1 Ordem sugerida
1. acertar **base delay + depth + rate**
2. acertar **HPF / LPF do wet**
3. acertar **mainW / crossW**
4. validar em estéreo
5. validar em mono
6. ajustar output trim / clipper

### 19.2 Critérios auditivos
O efeito está correto quando:
- abre a imagem sem parecer vibrato
- em mono, não colapsa de forma desagradável
- o ataque permanece centrado
- o wet “cola” no dry
- o modo 4 ainda soa musical

### 19.3 Sinais de erro
- modulação muito audível: rate/depth altos demais
- efeito metálico: LPF muito aberto
- som embolado: HPF do wet baixo demais
- centro instável: matriz estéreo agressiva demais
- sensação de plugin moderno: wet brilhante e muito limpo

---

## 20. Roadmap

### v1
Inclui:
- Mono-In / Stereo-Out
- 48 kHz / float32
- 1 buffer mono
- 2 taps fracionários
- Hermite
- LFO triangular suavizado
- HPF + LPF no wet
- 4 modos fixos
- soft clip final de proteção

### v2
Pode incluir:
- saturação leve no wet
- voicing mais refinado por modo
- controles contínuos ocultos
- assimetrias leves entre as vozes
- presets expandidos

### v3
Pode incluir:
- true stereo ou pseudo-true-stereo
- MIDI/USB mais sofisticado
- RTOS leve
- integração com outros blocos de efeitos

---

## 21. Erros comuns a evitar

- usar depth grande demais
- usar rate alto demais
- depender da interpolação linear
- wet sem filtragem
- tentar resolver headroom só com clipper
- recalcular filtros dentro do loop de amostra
- colocar operações bloqueantes no caminho do áudio
- assumir compatibilidade mono perfeita sem validar em escuta real
- transformar o modo 4 em chorus tradicional

---

## 22. Decisão final da v1

A arquitetura recomendada e congelada para a v1 é:

- **STM32H5**
- **48 kHz**
- **float32**
- **Mono-In / Stereo-Out**
- **DMA em blocos de 32 frames**
- **1 buffer circular mono de 1024 amostras**
- **2 taps fracionários modulados**
- **Hermite 3ª ordem**
- **LFO triangular suavizado**
- **oposição nominal de 180° entre as vozes**
- **HPF + LPF no wet**
- **dry gain fixo com headroom**
- **4 modos fixos**
- **smoothing por bloco**
- **soft clip final apenas como proteção**
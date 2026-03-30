# Dimension Chorus: Offline Validation Guide

Este guia documenta como executar e interpretar a suíte de testes numéricos (CTest) e a ferramenta de renderização de áudio (Offline Runner) para o firmware Dimension Chorus no STM32H5. A validação offline é **obrigatória** antes de testar em hardware.

## 1. Fluxo de Trabalho de Validação

O processo de validação offline consiste em duas partes que devem ser usadas em conjunto:
1. **CTest Numérico:** Verifica se as equações do DSP (clipping, atraso de taps, compatibilidade de mono) operam dentro das tolerâncias matemáticas exigidas.
2. **Offline Runner (Renderização em WAV):** Gera arquivos de áudio para audição humana (avaliação de artefatos, estéreo field) e inspeção via DAW, emitindo logs com o nível de pico (Max Peak Level) alcançado.

## 2. Compilando e Executando Testes

Na raiz do repositório, rode o build padrão CMake:

```bash
mkdir -p build && cd build
cmake ..
make
```

### Executar a Validação Matemática (CTest)
```bash
ctest -V
```

O CTest roda testes rigorosos sobre o `core/` (DSP puro):
- **Timing/Impulse Response:** Verifica se o tap de delay respeita o tamanho do buffer calculado em milissegundos, com pequena tolerância de fase (ex: `IMPULSE_TIMING_MARGIN_SAMPLES = 10` frames).
- **Headroom/Clipping:** Aplica overdrive (+6dB) ao input de uma senoide para garantir que a aproximação polinomial de *soft-clip* nunca deixe o output exceder o limite seguro definido (`SOFT_CLIP_MAX_THRESHOLD = 0.6667f`).
- **Mono Compatibility:** Realiza downmix de 100Hz e garante que a perda por cancelamento de fase na matriz (Cross W) não ultrapasse um limite RMS tolerável (`MONO_SUM_ATTENUATION_THRESHOLD = 0.5f` ou -6dB).

## 3. Ferramenta Offline Runner

O `offline_runner` processa sinais de teste completos em cada um dos modos do Dimension Chorus, salvando tanto o arquivo dry (seco) quanto as versões processadas estéreo e somadas em mono (para validar compatibilidade mono).

### Uso Básico

Sempre execute o offline runner a partir da **raiz do repositório**:

```bash
./build/tests/offline_runner --batch
```

O comando `--batch` vai renderizar sinais de `sine`, `sweep`, `impulse`, `noise` e `chord` em todos os 4 modos e salvá-los na pasta `tests/output/`.

Se quiser testar apenas um modo específico com um sinal:
```bash
./build/tests/offline_runner --mode=3 --signal=sweep
```

### Interpretando Logs de Pico (Peak Amplitude)

A partir da versão mais recente, a CLI vai gerar uma saída semelhante a:
```
[Mode 3 | Signal: sweep] Rendered. Max Peak: 0.8123 (Stereo), 0.7654 (Mono Sum)
```

Isso significa que, durante a simulação inteira de 5 segundos, nenhum dos canais L e R excedeu a amplitude `0.8123` e que o sum mono atingiu no máximo `0.7654`. Isso é fundamental para auditar "overflows" numéricos antes de rodar o código na DMA do STM32, já que a conversão float -> PCM lá assumirá limites rigorosos (clipping/wrapping em I2S causa ruído alto e desagradável).

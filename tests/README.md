# Dimension Chorus - Host Validation Suite

Esta pasta contém o ecossistema de testes *host-side* (rodando na máquina de desenvolvimento via CMake) focado em garantir a sanidade e robustez da matemática DSP antes que o firmware seja transportado para o STM32H5.

## Requisitos
- CMake (>= 3.10)
- GCC/Clang compatível com C99

## Compilando os Testes e o Runner

Na raiz do repositório:
```bash
mkdir build
cd build
cmake ..
make
```

## Executando os Testes Automatizados (CTest)

Após compilar:
```bash
cd build
ctest --output-on-failure
```
Isso validará o DSP Core contra limites de `headroom`, tempos de `delay` e perdas de `mono summing` dentro de um nível determinístico tolerável.

## Usando o Offline Runner (Geração de Áudio)

A suíte compila um executável CLI chamado `offline_runner`. Ele permite injetar sinais sintetizados (sine, sweep, noise, impulse, chord) direto no Core C99 e salvar o resultado como arquivos estéreo 16-bit PCM (WAV) na pasta `tests/output/`.

### Rodar um modo específico com um sinal
```bash
./build/tests/offline_runner --mode=1 --signal=sweep
```

### Rodar um render completo (Todos os Modos + Mono Sum + Todos os Sinais)
Isso é útil para comparação A/B entre commits:
```bash
./build/tests/offline_runner --batch
```
Isso povoará a pasta `tests/output/` com todos os cenários possíveis para audição crítica.
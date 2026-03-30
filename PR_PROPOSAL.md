# Proposta de PR: Suporte a Validação Offline com WAVs do Usuário

## 1. Diagnóstico do Estado Atual
O `offline_runner.c` atualmente gera sinais sintéticos limitados (`sine`, `sweep`, `impulse`, `noise`, `chord`) configurados para uma duração fixa (5 segundos). Não há suporte para carregar arquivos de áudio externos, o que impossibilita a validação auditiva real com guitarras, sintetizadores ou loops de bateria. Além disso, o CLI carece de opções para direcionamento específico de arquivo de saída e uma flag explícita para forçar a renderização do downmix mono fora do modo `--batch`.

## 2. Escopo Ideal do PR
Estender a suíte de validação do host para carregar arquivos WAV do usuário.
- **Manter core agnóstico:** Toda modificação ocorrerá na pasta `tests/`.
- **Simplicidade:** O processamento carregará o arquivo inteiro em memória (sem processamento em tempo real via streaming de disco), o que é suficiente e ideal para validação em uma máquina host.
- **Confiabilidade:** O parser WAV será simples, estrito a 16-bit PCM estéreo/mono a 48kHz.

## 3. Arquivos a Criar/Editar/Remover
- **Criar:**
  - `tests/wav_reader.h`
  - `tests/wav_reader.c`
- **Editar:**
  - `tests/offline_runner.c` (integração do leitor, nova CLI)
  - `tests/CMakeLists.txt` (adicionar `wav_reader.c` ao executável `offline_runner`)
  - `docs/OFFLINE_VALIDATION_GUIDE.md` (documentação das novas flags e uso de WAVs)

## 4. Proposta de CLI do Offline Runner
A CLI será retrocompatível mas estendida da seguinte forma:
```bash
  --input=<file.wav>   Usa arquivo WAV externo como entrada em vez de sinal sintético.
  --output=<file.wav>  Força um arquivo de saída específico (Ignorado se --batch).
  --mode=<1|2|3|4>     Modo de processamento (Padrão: 1).
  --signal=<type>      (Existente) sine, sweep, impulse, noise, chord. (Ignorado se --input existir).
  --mono-sum           Força a geração extra de um arquivo "monosum" no modo individual.
  --batch              Testa todos os modos (com os sintéticos originais ou com o --input fornecido).
```

## 5. Estratégia para Aceitar WAVs (Escopo Pequeno)
- Criar uma rotina `read_wav_file` simplificada: lê cabeçalho `RIFF/WAVE/fmt/data`.
- Rejeitar firmemente arquivos com sample rate diferente de 48000 Hz ou bits por sample diferente de 16.
- Se o arquivo for estéreo, fazer uma média (L+R)/2 automática durante o carregamento para gerar o `float* inMono` requerido pelo `DimensionChorus_ProcessBlock`.
- Alocar dinamicamente na RAM baseado no tamanho do chunk de dados (adequado para simulação de alguns segundos ou minutos na máquina host).

## 6. Organização no CMake/CTest
- `wav_reader.c` será atrelado **apenas** ao target executável `offline_runner`.
- O target `test_dsp_core` permanecerá puramente numérico e sintético para garantir o determinismo do CI (não exigindo dependência de arquivos WAV no repositório).

## 7. Conteúdo Inicial de docs/OFFLINE_VALIDATION_GUIDE.md
A documentação será expandida para incluir:
- **Testes com Arquivos Reais:** Explicar o fluxo com `--input` e `--output`.
- **Restrições de Áudio:** Documentar a exigência de 48kHz / 16-bit PCM.
- **Exemplos Práticos:**
  ```bash
  # Processa guitarra em modo 2, exportando mix estéreo e mono-sum
  ./build/tests/offline_runner --input=guitar.wav --output=guitar_mode2.wav --mode=2 --mono-sum
  ```

## 8. Ordem dos Commits
1. 🧹 `tests: Add lightweight WAV reader for 16-bit PCM files`
2. ✨ `tests: Add --input and --output CLI parameters to offline_runner`
3. ✨ `tests: Implement user WAV processing and --mono-sum flag in offline_runner`
4. 📝 `docs: Update OFFLINE_VALIDATION_GUIDE with custom WAV processing instructions`

## 9. O Que Deixar Fora de Escopo
- **Resampling/Conversão Automática:** Nada de bibliotecas como `libsamplerate`. Arquivo que não seja 48kHz/16-bit simplesmente falhará com erro claro.
- **Suporte a 24-bit, 32-bit float ou arquivos compactados (MP3, FLAC).**
- **Testes CTest baseados em WAVs:** CI continuará puramente sintético.
- **Mudanças no firmware (`core/`, `app/`):** Nenhuma linha de produção será alterada.

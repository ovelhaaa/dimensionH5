# TEST PLAN

## Offline DSP
- Impulse (Dirac delta) para validar tempos exatos de delay e estabilidade dos filtros Biquad DF2T.
- Sine 440 Hz / Sine 1 kHz para avaliar Headroom, Clipping polinomial macio (sem travar em NaN).
- Log sweep para confirmar ação do Low Pass e High Pass no sinal molhado (wet).
- White/pink noise para balanço global RMS In/Out mantendo o unity gain.
- Mode switching (Troca de modos em tempo de processamento) sem ruídos bruscos (zipper/clicks).
- Mono sum (Soma L+R em -6dB) para validar compatibilidade mono sem vales fixos de comb-filter agressivos.

## Hardware Bring-up
- Passthrough validation (ADC direto ao DAC) para garantir configurações I2S/SAI limpas (sem DSP).
- Alinhamento de 24-bits empacotados em 32-bits lidos/escritos no DMA.
- Integridade das callbacks de half-transfer e full-transfer do DMA sem perda de buffer.
- D-cache / MPU validation (Garantir que os buffers de DMA estão em região Non-Cacheable da SRAM).
- CPU time budget (Medir via pino de Profile GPIO Toggle na entrada e saída do superloop).

## Acceptance
- Sem clicks ou estalos óbvios em operação normal.
- Sem overruns ou dropout em blocos no superloop.
- Saída estéreo estável, espacial, fiel ao chorus analógico original.
- Todos os 4 modos operam corretamente e com a transição fluída.

# V1 Scope

## In scope
- Stereo Dimension-style chorus
- 4 preset modes (Modulação via LFO Hermite-interpolated e matriz Stereo Crossmix)
- Desktop WAV render path (`tests/render_wav_main.c` sem dep. externas como Python, outputting 16-bit PCM estéreo 48kHz WAVs)
- STM32H5 real-time audio (SRAM pura, sem interrupção bloquando, processamento DMA `AUDIO_BLOCK_FRAMES`)
- Basic mode switching (Soft clipping and smooth parameter transitions per-block)
- Basic docs and demos (Testes isolados via CMake e CTest)
- Offline DSP Validation tests and Continuous Integration (GitHub Actions)

## Out of scope
- Display UI (TFT, OLED)
- MIDI
- Preset storage (Flash save)
- Multi-effect engine (Reverb, Delay)
- Advanced editor
- Product industrialization (Gerber de PCB final ou enclosure)
- Codec Configuration na interface de DSP ou App (App e Plataforma estritamente separados)

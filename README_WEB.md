# Dimension Chorus - Web Port

This document details the architecture and implementation of the Web (WebAssembly) port of the Dimension Chorus DSP, fulfilling the goal of sharing the exact same DSP core between the STM32H5 microcontroller and the browser.

## Architecture Guidelines

The port follows a strict separation of concerns, heavily inspired by the `te2350` architecture:

1. **`core/` (Unchanged)**: Contains the pure, hardware-agnostic C DSP core (`dimension_chorus.c`, `dsp_biquad.c`, etc.). It does not know about Emscripten, Web Audio, or STM32 HAL.
2. **`app/`**: Contains bridging logic for STM32. We intentionally leave it alone for the web build, as Web Audio naturally processes float32 data, removing the need for `app/audio_engine.c`'s `int32_t` <-> `float32` packing/unpacking routines.
3. **`platform/stm32h5/` (Unchanged)**: Contains the STM32H5 specific implementations (DMA, codec setup).
4. **`platform/web/` (New)**: Contains the Web Audio implementation and Emscripten bindings.

## Web Platform Structure (`platform/web/`)

- `wasm_wrapper.c`: Exposes a minimal, stable C API to JavaScript. It initializes the core, handles memory buffers, and processes audio blocks using the exact `DSP_BLOCK_SIZE` expected by the core.
- `build.sh`: A simple bash script to invoke Emscripten (`emcc`) to compile the DSP core and `wasm_wrapper.c` into WebAssembly.
- `index.html`: The UI for the web port.
- `main.js`: Handles user interaction and initializes the Web Audio API. It fetches the WASM binary and sends it to the AudioWorklet.
- `worklet.js`: Runs in a separate audio thread via the Web Audio API `AudioWorkletProcessor`. It instantiates the WASM module and loops over incoming audio, passing it to WASM in chunks of `DSP_BLOCK_SIZE`.

## How to Build Locally

You need Emscripten (`emcc`) installed. Alternatively, you can run the GitHub Actions workflow.

1. Install Emscripten SDK (emsdk).
2. Navigate to `platform/web/`.
3. Run `./build.sh`.
4. The output will be placed in `platform/web/dist/`.

## How to Test

Since Web Audio requires a secure context (or localhost) and the script relies on ES Modules and Fetch API, you must serve the files with a local HTTP server.

```bash
cd platform/web
python3 -m http.server 8000
```

Open `http://localhost:8000` in your browser.

## Publishing & CI

A GitHub Actions workflow is provided in `.github/workflows/web.yml`.
On every push to `main`, it will:
1. Setup Emscripten.
2. Build the WASM binary.
3. Deploy `index.html`, `main.js`, `worklet.js`, and the `dist/` directory to GitHub Pages.

## Limitations & Future Work

- **Sample Rate**: The DSP core uses a fixed macro `DSP_SAMPLE_RATE` of 48000.0f. The web application currently enforces this rate via `AudioContext({ sampleRate: 48000 })`. If the browser cannot native operate at this rate, the Web Audio API handles resampling automatically before it reaches our WASM code.
- **Buffer Sizes**: `AudioWorkletProcessor` receives frames in batches of 128 (typically). Our C code expects exactly 32. `worklet.js` handles this cleanly by chunking the 128 frames into four calls of 32 frames to `dimension_process()`.
- **Inputs**: Currently, the demo includes a simple Test Tone generator (sawtooth oscillator). Future work can add file upload functionality or `getUserMedia` for live microphone/guitar input.

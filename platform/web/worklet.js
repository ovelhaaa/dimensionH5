import Module from './dist/dimension_chorus.js';

class DimensionChorusWorklet extends AudioWorkletProcessor {
    constructor() {
        super();
        this.wasmModule = null;
        this.initialized = false;

        // Pointers for JS <-> WASM shared memory
        this.inPtr = 0;
        this.outPtr = 0;
        this.blockSize = 32; // Defined in core/dsp_common.h

        // Buffer index for partial blocks (if Web Audio block size != 32)
        // Note: Web Audio block size is usually 128
        this.bufferIndex = 0;

        // Message port listener
        this.port.onmessage = (event) => {
            const data = event.data;
            if (data.type === 'init') {
                this.initWasm(data.wasmBytes);
            } else if (data.type === 'setMode') {
                if (this.initialized) {
                    this.wasmModule._dimension_set_mode(data.mode);
                }
            } else if (data.type === 'setSelectionMode') {
                if (this.initialized) {
                    this.wasmModule._dimension_set_selection_mode(data.selMode);
                }
            } else if (data.type === 'setModeMask') {
                if (this.initialized) {
                    this.wasmModule._dimension_set_mode_mask(data.mask);
                }
            } else if (data.type === 'reset') {
                if (this.initialized) {
                    this.wasmModule._dimension_reset();
                }
            } else if (data.type === 'enableCustomParams') {
                if (this.initialized) this.wasmModule._dimension_enable_custom_params(data.enabled ? 1 : 0);
            } else if (data.type === 'loadBaseMode') {
                if (this.initialized) {
                    this.wasmModule._dimension_load_base_mode(data.mode);
                    this.syncCustomParamsToMain();
                }
            } else if (data.type === 'setCustomParam') {
                if (this.initialized) {
                    switch (data.param) {
                        case 'rateHz': this.wasmModule._dimension_set_rate(data.value); break;
                        case 'baseMs': this.wasmModule._dimension_set_base_ms(data.value); break;
                        case 'depthMs': this.wasmModule._dimension_set_depth_ms(data.value); break;
                        case 'mainWet': this.wasmModule._dimension_set_main_wet(data.value); break;
                        case 'crossWet': this.wasmModule._dimension_set_cross_wet(data.value); break;
                        case 'hpf1Hz': this.wasmModule._dimension_set_hpf_hz(1, data.value); break;
                        case 'hpf2Hz': this.wasmModule._dimension_set_hpf_hz(2, data.value); break;
                        case 'lpf1Hz': this.wasmModule._dimension_set_lpf_hz(1, data.value); break;
                        case 'lpf2Hz': this.wasmModule._dimension_set_lpf_hz(2, data.value); break;
                        case 'wet1Gain': this.wasmModule._dimension_set_wet_gain(1, data.value); break;
                        case 'wet2Gain': this.wasmModule._dimension_set_wet_gain(2, data.value); break;
                        case 'baseOffset2Ms': this.wasmModule._dimension_set_base_offset2_ms(data.value); break;
                        case 'depth2Scale': this.wasmModule._dimension_set_depth2_scale(data.value); break;
                    }
                }
            } else if (data.type === 'getCustomParams') {
                if (this.initialized) {
                    this.syncCustomParamsToMain();
                }
            }
        };
    }

    syncCustomParamsToMain() {
        const params = {
            rateHz: this.wasmModule._dimension_get_rate(),
            baseMs: this.wasmModule._dimension_get_base_ms(),
            depthMs: this.wasmModule._dimension_get_depth_ms(),
            mainWet: this.wasmModule._dimension_get_main_wet(),
            crossWet: this.wasmModule._dimension_get_cross_wet(),
            hpf1Hz: this.wasmModule._dimension_get_hpf_hz(1),
            lpf1Hz: this.wasmModule._dimension_get_lpf_hz(1),
            hpf2Hz: this.wasmModule._dimension_get_hpf_hz(2),
            lpf2Hz: this.wasmModule._dimension_get_lpf_hz(2),
            wet1Gain: this.wasmModule._dimension_get_wet_gain(1),
            wet2Gain: this.wasmModule._dimension_get_wet_gain(2),
            baseOffset2Ms: this.wasmModule._dimension_get_base_offset2_ms(),
            depth2Scale: this.wasmModule._dimension_get_depth2_scale()
        };
        this.port.postMessage({ type: 'customParamsUpdate', params });
    }

    async initWasm(wasmBytes) {
        try {
            // Emscripten AudioWorklet hack: some browsers lack URL in worklets
            if (typeof URL === 'undefined') {
                globalThis.URL = class URL {
                    constructor(url, base) {
                        this.href = url;
                    }
                };
            }

            // Instantiate the WASM module
            // We pass the raw bytes loaded by the main thread.
            const moduleConfig = {
                wasmBinary: wasmBytes,
            };

            this.wasmModule = await Module(moduleConfig);

            // Initialize DSP
            this.wasmModule._dimension_init();

            // Get buffer pointers configured in WASM
            this.inPtr = this.wasmModule._dimension_get_in_buffer();
            this.outPtr = this.wasmModule._dimension_get_out_buffer();
            this.blockSize = this.wasmModule._dimension_get_block_size();

            // Wait for WASM memory to be ready if it's not exported directly by the module
            // In Emscripten, `HEAPF32` is usually available on the module instance.
            // If it's not, we might need to access the memory from `this.wasmModule.asm.memory`
            // or just `this.wasmModule.HEAPF32`. Let's ensure we get the right buffer.
            const buffer = this.wasmModule.HEAPF32 ? this.wasmModule.HEAPF32.buffer : this.wasmModule.wasmMemory.buffer;

            // Use a JS typed array view over WASM memory
            this.inBuffer = new Float32Array(buffer, this.inPtr, this.blockSize);
            this.outBuffer = new Float32Array(buffer, this.outPtr, this.blockSize * 2); // Stereo out

            this.initialized = true;
            this.port.postMessage({ type: 'initialized' });
        } catch (e) {
            console.error(e);
            this.port.postMessage({ type: 'error', message: e.toString() });
        }
    }

    process(inputs, outputs, parameters) {
        if (!this.initialized) {
            return true; // Wait for WASM
        }

        const input = inputs[0];  // Only one input node
        const output = outputs[0]; // Only one output node

        // Handle unconnected input
        if (!input || input.length === 0 || input[0].length === 0) {
            return true;
        }

        const inputChannel = input[0];
        const outL = output[0];
        const outR = output[1] || output[0]; // Fallback to mono out if node only has 1 ch

        const framesToProcess = inputChannel.length;

        for (let i = 0; i < framesToProcess; i += this.blockSize) {
            // Fill WASM input buffer (Mono)
            for (let j = 0; j < this.blockSize; j++) {
                if (i + j < framesToProcess) {
                    this.inBuffer[j] = inputChannel[i + j];
                } else {
                    this.inBuffer[j] = 0.0;
                }
            }

            // Call DSP
            this.wasmModule._dimension_process();

            // Read WASM output buffer (Stereo interleaved L-R)
            for (let j = 0; j < this.blockSize; j++) {
                if (i + j < framesToProcess) {
                    outL[i + j] = this.outBuffer[j * 2];
                    if (output.length > 1) {
                        outR[i + j] = this.outBuffer[j * 2 + 1];
                    }
                }
            }
        }

        return true; // Keep processor alive
    }
}

registerProcessor('dimension-chorus-worklet', DimensionChorusWorklet);

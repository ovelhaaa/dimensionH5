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
            if (!this.initialized && data.type !== 'init') return;

            if (data.type === 'init') {
                this.initWasm(data.wasmBytes);
            } else if (data.type === 'setMode') {
                this.wasmModule._dimension_set_mode(data.mode);
            } else if (data.type === 'setSelectionMode') {
                this.wasmModule._dimension_set_selection_mode(data.mode);
            } else if (data.type === 'setModeMask') {
                this.wasmModule._dimension_set_mode_mask(data.mask);
            } else if (data.type === 'reset') {
                this.wasmModule._dimension_reset();
            } else if (data.type === 'getTelemetry') {
                this.port.postMessage({
                    type: 'telemetry',
                    rate: this.wasmModule._dimension_get_rate(),
                    depth: this.wasmModule._dimension_get_depth(),
                    base: this.wasmModule._dimension_get_base(),
                    mainW: this.wasmModule._dimension_get_mainw(),
                    crossW: this.wasmModule._dimension_get_crossw()
                });
            }
        };
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

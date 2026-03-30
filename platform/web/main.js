let audioContext;
let chorusNode;

async function initAudio() {
    if (audioContext) return; // Already initialized

    // Create an AudioContext. We'll set sample rate to 48000 to match the DSP
    // although it can handle resampling if needed.
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    audioContext = new AudioContext({ sampleRate: 48000 });

    // Ensure it's running (some browsers suspend it)
    if (audioContext.state === 'suspended') {
        await audioContext.resume();
    }

    // Load Emscripten glue JS and WASM binary for worklet
    const response = await fetch('./dist/dimension_chorus.wasm');
    const wasmBytes = await response.arrayBuffer();

    // Register our AudioWorklet
    await audioContext.audioWorklet.addModule('./worklet.js');

    // Create Worklet Node
    chorusNode = new AudioWorkletNode(audioContext, 'dimension-chorus-worklet', {
        numberOfInputs: 1,
        numberOfOutputs: 1,
        outputChannelCount: [2] // We expect stereo out
    });

    // Initialize WASM inside the Worklet
    chorusNode.port.postMessage({ type: 'init', wasmBytes: wasmBytes });

    // Handle initialization response
    chorusNode.port.onmessage = (e) => {
        if (e.data.type === 'initialized') {
            document.getElementById('status').innerText = 'Audio Engine Ready';
            document.getElementById('start-btn').disabled = true;
            document.getElementById('play-btn').disabled = false;
        } else if (e.data.type === 'error') {
            console.error('Worklet Error:', e.data.message);
            document.getElementById('status').innerText = 'Error: ' + e.data.message;
        }
    };

    // Connect Node to Destination
    chorusNode.connect(audioContext.destination);
}

function setMode(mode) {
    if (chorusNode) {
        chorusNode.port.postMessage({ type: 'setMode', mode: parseInt(mode, 10) });
    }
}

// Simple test sine wave player
let oscillator;
function toggleOscillator() {
    if (!audioContext || !chorusNode) return;

    if (oscillator) {
        oscillator.stop();
        oscillator.disconnect();
        oscillator = null;
        document.getElementById('play-btn').innerText = 'Play Test Tone';
        return;
    }

    oscillator = audioContext.createOscillator();
    oscillator.type = 'sawtooth';
    oscillator.frequency.value = 440; // A4
    oscillator.connect(chorusNode);
    oscillator.start();
    document.getElementById('play-btn').innerText = 'Stop Test Tone';
}

// User interactions
document.getElementById('start-btn').addEventListener('click', initAudio);

const modeRadios = document.querySelectorAll('input[name="mode"]');
modeRadios.forEach(radio => {
    radio.addEventListener('change', (e) => setMode(e.target.value));
});

document.getElementById('play-btn').addEventListener('click', toggleOscillator);

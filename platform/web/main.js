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

// Audio file playback state
let decodedBuffer = null;
let currentSource = null;
let isPlaying = false;

// UI Elements
const audioUpload = document.getElementById('audio-upload');
const playOrigBtn = document.getElementById('play-orig-btn');
const playProcBtn = document.getElementById('play-proc-btn');
const stopBtn = document.getElementById('stop-btn');
const fileInfo = document.getElementById('file-info');
const fileNameDisplay = document.getElementById('file-name');
const fileDetailsDisplay = document.getElementById('file-details');
const uploadError = document.getElementById('upload-error');

function handleFileUpload(event) {
    if (!audioContext) {
        uploadError.innerText = 'Please click "Power On" first.';
        uploadError.style.display = 'block';
        return;
    }

    const file = event.target.files[0];
    if (!file) return;

    uploadError.style.display = 'none';

    // Disable buttons and input during load
    audioUpload.disabled = true;
    playOrigBtn.disabled = true;
    playProcBtn.disabled = true;
    stopBtn.disabled = true;
    decodedBuffer = null;

    fileInfo.style.display = 'block';
    fileNameDisplay.innerText = `Loading: ${file.name}...`;
    fileDetailsDisplay.innerText = '';

    const reader = new FileReader();
    reader.onload = async (e) => {
        try {
            const arrayBuffer = e.target.result;
            // Decode the audio data
            decodedBuffer = await audioContext.decodeAudioData(arrayBuffer);

            // Enable playback buttons
            playOrigBtn.disabled = false;
            playProcBtn.disabled = false;

            // Display info
            fileNameDisplay.innerText = `File: ${file.name}`;
            const durationSec = decodedBuffer.duration.toFixed(2);
            fileDetailsDisplay.innerText = `${decodedBuffer.sampleRate} Hz | ${decodedBuffer.numberOfChannels} ch | ${durationSec}s`;

        } catch (error) {
            console.error('Error decoding audio:', error);
            uploadError.innerText = `Failed to decode audio. Format might not be supported.`;
            uploadError.style.display = 'block';
            fileNameDisplay.innerText = '';
            fileInfo.style.display = 'none';
        }
    };

    reader.onerror = () => {
        uploadError.innerText = 'Failed to read file.';
        uploadError.style.display = 'block';
    };

    // Check if the file is an mp3. Depending on the browser, some don't support mp3 in Web Audio API decodeAudioData out of the box.
    // However, most modern browsers do. If this fails, the catch block above will show an error.
    reader.readAsArrayBuffer(file);
}

function stopAudio() {
    if (currentSource) {
        try { currentSource.stop(); } catch (e) {} // Prevent crash if already stopped
        currentSource.disconnect();
        currentSource = null;
    }
    if (currentDownmixer) {
        currentDownmixer.disconnect();
        currentDownmixer = null;
    }
    isPlaying = false;
    stopBtn.disabled = true;
    playOrigBtn.style.opacity = '1';
    playProcBtn.style.opacity = '1';
}

function playBuffer(processAudio) {
    if (!audioContext || !decodedBuffer) return;

    // Gate processed playback on chorusNode availability
    if (processAudio && !chorusNode) {
        uploadError.innerText = 'Audio processor is not ready yet. Please wait.';
        uploadError.style.display = 'block';
        return;
    }

    uploadError.style.display = 'none';
    stopAudio(); // Stop currently playing audio if any

    currentSource = audioContext.createBufferSource();
    currentSource.buffer = decodedBuffer;

    if (processAudio) {
        // We use a GainNode to explicitly downmix to mono if the input is stereo
        // because the hardware / Dimension Chorus DSP expects a mono input signal.
        // It processes the mono input and generates a stereo output.
        const downmixer = audioContext.createGain();
        downmixer.channelCount = 1;
        downmixer.channelCountMode = 'explicit';

        currentSource.connect(downmixer);
        downmixer.connect(chorusNode);

        playProcBtn.style.opacity = '0.5';
    } else {
        // Original - connect directly to destination
        currentSource.connect(audioContext.destination);
        playOrigBtn.style.opacity = '0.5';
    }

    currentSource.onended = () => {
        stopAudio();
    };

    currentSource.start(0);
    isPlaying = true;
    stopBtn.disabled = false;
}

audioUpload.addEventListener('change', handleFileUpload);
playOrigBtn.addEventListener('click', () => playBuffer(false));
playProcBtn.addEventListener('click', () => playBuffer(true));
stopBtn.addEventListener('click', stopAudio);

// Simple test sine wave player
let oscillator;
function toggleOscillator() {
    if (!audioContext || !chorusNode) return;

    if (oscillator) {
        oscillator.stop();
        oscillator.disconnect();
        oscillator = null;
        document.getElementById('play-btn').innerText = 'Debug: Test Tone (A4)';
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

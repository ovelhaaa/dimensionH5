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

            // Start telemetry polling
            setInterval(() => {
                if (chorusNode) {
                    chorusNode.port.postMessage({ type: 'getTelemetry' });
                }
            }, 100);

            // Force initial sync
            updateBehaviorMode();

        } else if (e.data.type === 'error') {
            console.error('Worklet Error:', e.data.message);
            document.getElementById('status').innerText = 'Error: ' + e.data.message;
        } else if (e.data.type === 'telemetry') {
            updateTelemetryUI(e.data);
        }
    };

    // Connect Node to Destination
    chorusNode.connect(audioContext.destination);
}

// Global UI State
let isComboMode = false;

function updateTelemetryUI(data) {
    document.getElementById('tel-rate').innerText = data.rate.toFixed(2) + ' Hz';
    document.getElementById('tel-depth').innerText = data.depth.toFixed(2) + ' ms';
    document.getElementById('tel-base').innerText = data.base.toFixed(2) + ' ms';
    document.getElementById('tel-mainw').innerText = data.mainW.toFixed(2);
    document.getElementById('tel-crossw').innerText = data.crossW.toFixed(2);
}

function syncModeStateToDSP() {
    if (!chorusNode) return;

    if (isComboMode) {
        // Build bitmask from checkboxes
        const checkboxes = document.querySelectorAll('input[name="mode"]');
        let mask = 0;
        checkboxes.forEach((cb, idx) => {
            if (cb.checked) mask |= (1 << idx);
        });

        // Prevent all-off state in combo mode visually
        if (mask === 0) {
            mask = 1;
            document.getElementById('mode0').checked = true;
        }

        chorusNode.port.postMessage({ type: 'setModeMask', mask: mask });
    } else {
        // Authentic mode: use standard setMode
        const selected = document.querySelector('input[name="mode"]:checked');
        if (selected) {
            chorusNode.port.postMessage({ type: 'setMode', mode: parseInt(selected.value, 10) });
        }
    }
}

function setMode(mode) {
    // Handling click directly via syncModeStateToDSP on change event
}

function updateBehaviorMode() {
    const selector = document.getElementById('behavior-mode');
    const desc = document.getElementById('behavior-desc');
    const modeInputs = document.querySelectorAll('input[name="mode"]');

    isComboMode = (selector.value === "1");

    if (isComboMode) {
        desc.innerText = "Expanded behavior: Select multiple modes simultaneously.";
        // Change inputs to checkboxes to allow multiple selections
        modeInputs.forEach(input => {
            input.type = 'checkbox';
        });

        // Ensure at least one is selected if we just switched to combo
        const checked = document.querySelectorAll('input[name="mode"]:checked');
        if (checked.length === 0) {
            document.getElementById('mode0').checked = true;
        }

        if (chorusNode) {
            chorusNode.port.postMessage({ type: 'setSelectionMode', mode: 1 /* COMBO */ });
        }
    } else {
        desc.innerText = "Classic behavior: Only one mode active at a time.";

        // Find highest selected to keep as the active radio button
        let highestVal = 0;
        modeInputs.forEach(input => {
            if (input.checked) highestVal = Math.max(highestVal, parseInt(input.value, 10));
        });

        // Change inputs back to radio
        modeInputs.forEach(input => {
            input.type = 'radio';
            input.checked = (parseInt(input.value, 10) === highestVal);
        });

        if (chorusNode) {
            chorusNode.port.postMessage({ type: 'setSelectionMode', mode: 0 /* SINGLE */ });
        }
    }

    syncModeStateToDSP();
}

// Audio file playback state
let decodedBuffer = null;
let currentSource = null;
let currentDownmixer = null;

let playbackMode = "original"; // "original" or "processed"
let playbackState = "stopped"; // "stopped", "playing", "paused"
let startTime = 0;
let pauseOffset = 0;
let duration = 0;
let animationFrameId = null;

// UI Elements
const audioUpload = document.getElementById('audio-upload');
const fileInfo = document.getElementById('file-info');
const fileNameDisplay = document.getElementById('file-name');
const fileDetailsDisplay = document.getElementById('file-details');
const uploadError = document.getElementById('upload-error');

const toggleContainer = document.getElementById('playback-toggle-container');
const toggleOrig = document.getElementById('toggle-orig');
const toggleProc = document.getElementById('toggle-proc');

const transportControls = document.getElementById('transport-controls');
const playPauseBtn = document.getElementById('play-pause-btn');
const stopBtn = document.getElementById('stop-btn');

const progressContainer = document.getElementById('progress-container');
const progressBar = document.getElementById('progress-bar');
const timeCurrent = document.getElementById('time-current');
const timeTotal = document.getElementById('time-total');

function formatTime(seconds) {
    const min = Math.floor(seconds / 60);
    const sec = Math.floor(seconds % 60);
    return `${min}:${sec < 10 ? '0' : ''}${sec}`;
}

function updateProgressUI() {
    if (playbackState === "playing") {
        const currentTime = audioContext.currentTime - startTime + pauseOffset;
        if (currentTime >= duration) {
            // Let the onended handler take care of stopping
            progressBar.value = duration;
            timeCurrent.innerText = formatTime(duration);
            return;
        }
        progressBar.value = currentTime;
        timeCurrent.innerText = formatTime(currentTime);
        animationFrameId = requestAnimationFrame(updateProgressUI);
    }
}

function resetProgressUI() {
    progressBar.value = 0;
    timeCurrent.innerText = formatTime(0);
    if (animationFrameId) {
        cancelAnimationFrame(animationFrameId);
        animationFrameId = null;
    }
}

function handleFileUpload(event) {
    if (!audioContext) {
        uploadError.innerText = 'Please click "Power On" first.';
        uploadError.style.display = 'block';
        return;
    }

    const file = event.target.files[0];
    if (!file) return;

    uploadError.style.display = 'none';

    // Stop and reset current playback
    stopAudio();

    // Disable input during load
    audioUpload.disabled = true;
    decodedBuffer = null;
    duration = 0;

    fileInfo.style.display = 'block';
    fileNameDisplay.innerText = `Loading: ${file.name}...`;
    fileDetailsDisplay.innerText = '';

    // Hide controls until loaded
    toggleContainer.style.display = 'none';
    transportControls.style.display = 'none';
    progressContainer.style.display = 'none';

    const reader = new FileReader();
    reader.onload = async (e) => {
        try {
            const arrayBuffer = e.target.result;
            // Decode the audio data
            decodedBuffer = await audioContext.decodeAudioData(arrayBuffer);
            duration = decodedBuffer.duration;

            // Setup UI
            audioUpload.disabled = false;
            fileNameDisplay.innerText = `File: ${file.name}`;
            const durationSec = duration.toFixed(2);
            fileDetailsDisplay.innerText = `${decodedBuffer.sampleRate} Hz | ${decodedBuffer.numberOfChannels} ch | ${durationSec}s`;

            progressBar.max = duration;
            timeTotal.innerText = formatTime(duration);
            resetProgressUI();

            // Show controls
            toggleContainer.style.display = 'flex';
            transportControls.style.display = 'flex';
            progressContainer.style.display = 'flex';
            playPauseBtn.disabled = false;
            stopBtn.disabled = true;
            playPauseBtn.innerText = 'Play';
            playPauseBtn.classList.remove('playing');

        } catch (error) {
            console.error('Error decoding audio:', error);
            uploadError.innerText = `Failed to decode audio. Format might not be supported.`;
            uploadError.style.display = 'block';
            fileNameDisplay.innerText = '';
            fileInfo.style.display = 'none';
            audioUpload.disabled = false;
        }
    };

    reader.onerror = () => {
        uploadError.innerText = 'Failed to read file.';
        uploadError.style.display = 'block';
        audioUpload.disabled = false;
    };

    reader.readAsArrayBuffer(file);
}

function cleanupAudioNodes() {
    if (currentSource) {
        currentSource.onended = null;
        try { currentSource.stop(); } catch (e) {}
        currentSource.disconnect();
        currentSource = null;
    }
    if (currentDownmixer) {
        currentDownmixer.disconnect();
        currentDownmixer = null;
    }
}

function stopAudio() {
    cleanupAudioNodes();
    playbackState = "stopped";
    pauseOffset = 0;

    stopBtn.disabled = true;
    playPauseBtn.innerText = 'Play';
    playPauseBtn.classList.remove('playing');
    resetProgressUI();
}

async function startPlayback(offset) {
    if (!audioContext || !decodedBuffer) return;

    if (audioContext.state === 'suspended') {
        await audioContext.resume();
    }

    if (playbackMode === "processed" && !chorusNode) {
        console.warn('Playback blocked: Audio processor not ready.');
        uploadError.innerText = 'Audio processor is not ready yet. Please wait.';
        uploadError.style.display = 'block';
        return;
    }

    uploadError.style.display = 'none';
    cleanupAudioNodes();

    currentSource = audioContext.createBufferSource();
    currentSource.buffer = decodedBuffer;

    if (playbackMode === "processed") {
        currentDownmixer = audioContext.createGain();
        currentDownmixer.channelCount = 1;
        currentDownmixer.channelCountMode = 'explicit';

        currentSource.connect(currentDownmixer);
        currentDownmixer.connect(chorusNode);
    } else {
        currentSource.connect(audioContext.destination);
    }

    // Handle end of playback
    currentSource.onended = () => {
        // Only stop if we actually reached the end naturally, not because we called stop() or seeked
        if (playbackState === "playing") {
            const currentTime = audioContext.currentTime - startTime + pauseOffset;
            if (currentTime >= duration - 0.1) { // 100ms margin
                stopAudio();
            }
        }
    };

    // Calculate start time based on context time
    startTime = audioContext.currentTime;
    pauseOffset = offset;

    currentSource.start(0, offset);
    playbackState = "playing";

    playPauseBtn.innerText = 'Pause';
    playPauseBtn.classList.add('playing');
    stopBtn.disabled = false;

    // Start animation loop for progress bar
    if (animationFrameId) cancelAnimationFrame(animationFrameId);
    animationFrameId = requestAnimationFrame(updateProgressUI);
}

function togglePlayPause() {
    if (!decodedBuffer) return;

    if (playbackState === "playing") {
        // Pause
        pauseOffset += audioContext.currentTime - startTime;
        cleanupAudioNodes();
        playbackState = "paused";

        playPauseBtn.innerText = 'Play';
        playPauseBtn.classList.remove('playing');
        if (animationFrameId) {
            cancelAnimationFrame(animationFrameId);
            animationFrameId = null;
        }
    } else if (playbackState === "paused") {
        // Resume
        startPlayback(pauseOffset);
    } else {
        // Play from start
        startPlayback(0);
    }
}

function handleSeek(event) {
    if (!decodedBuffer) return;

    const seekTime = parseFloat(event.target.value);
    timeCurrent.innerText = formatTime(seekTime);

    if (playbackState === "playing") {
        startPlayback(seekTime);
    } else {
        pauseOffset = seekTime;
    }
}

function handleModeChange(event) {
    playbackMode = event.target.value;

    if (playbackState === "playing") {
        // Seamlessly switch mode by restarting playback at current time
        const currentTime = audioContext.currentTime - startTime + pauseOffset;
        startPlayback(currentTime);
    }
}

audioUpload.addEventListener('change', handleFileUpload);
playPauseBtn.addEventListener('click', togglePlayPause);
stopBtn.addEventListener('click', stopAudio);
progressBar.addEventListener('input', handleSeek); // 'input' fires while dragging

const playbackModeRadios = document.querySelectorAll('input[name="playback-mode"]');
playbackModeRadios.forEach(radio => {
    radio.addEventListener('change', handleModeChange);
});

// Simple test sine wave player
let oscillator;
async function toggleOscillator() {
    if (!audioContext || !chorusNode) return;

    if (audioContext.state === 'suspended') {
        await audioContext.resume();
    }

    if (oscillator) {
        try { oscillator.stop(); } catch (e) {}
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

const modeInputs = document.querySelectorAll('input[name="mode"]');
modeInputs.forEach(input => {
    input.addEventListener('change', syncModeStateToDSP);
});

document.getElementById('behavior-mode').addEventListener('change', updateBehaviorMode);

document.getElementById('play-btn').addEventListener('click', toggleOscillator);

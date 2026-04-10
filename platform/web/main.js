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

            // Sync UI state to DSP
            setSelectionMode(currentSelectionMode);
            if (currentSelectionMode === 0) {
                const checkedBoxes = Array.from(modeCheckboxes).filter(cb => cb.checked);
                if (checkedBoxes.length > 0) {
                    setMode(checkedBoxes[0].value);
                }
            } else {
                updateMask();
            }
            // Sync UI state to DSP
            setSelectionMode(currentSelectionMode);
            if (currentSelectionMode === 0) {
                const checkedBoxes = Array.from(modeCheckboxes).filter(cb => cb.checked);
                if (checkedBoxes.length > 0) {
                    setMode(checkedBoxes[0].value);
                    loadBaseMode(checkedBoxes[0].value);
                }
            } else {
                updateMask();
            }
        } else if (e.data.type === 'customParamsUpdate') {
            currentDSPParams = e.data.params;
            updateUIFromParams(e.data.params);
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

function setSelectionMode(selMode) {
    if (chorusNode) {
        chorusNode.port.postMessage({ type: 'setSelectionMode', selMode: parseInt(selMode, 10) });
    }
}

function setModeMask(mask) {
    if (chorusNode) {
        chorusNode.port.postMessage({ type: 'setModeMask', mask: parseInt(mask, 10) });
    }
}

function enableCustomParams(enabled) {
    if (chorusNode) {
        chorusNode.port.postMessage({ type: 'enableCustomParams', enabled: enabled });
    }
}

function loadBaseMode(mode) {
    if (chorusNode) {
        chorusNode.port.postMessage({ type: 'loadBaseMode', mode: parseInt(mode, 10) });
    }
}

function setCustomParam(param, value) {
    if (chorusNode) {
        chorusNode.port.postMessage({ type: 'setCustomParam', param: param, value: parseFloat(value) });
    }
}

function getCustomParams() {
    if (chorusNode) {
        chorusNode.port.postMessage({ type: 'getCustomParams' });
    }
}

// Audio file playback state
let decodedBuffer = null;
let currentSource = null;
let currentDownmixer = null;

let playbackMode = "original"; // "original" or "processed"
let playbackState = "stopped"; // "stopped", "playing", "paused"
let repeatEnabled = false;
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
const repeatBtn = document.getElementById('repeat-btn');

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
        const elapsedTime = audioContext.currentTime - startTime + pauseOffset;
        const currentTime = repeatEnabled && duration > 0
            ? elapsedTime % duration
            : elapsedTime;
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
    currentSource.loop = repeatEnabled;

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
        if (repeatEnabled) return;
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

function updateRepeatButtonUI() {
    repeatBtn.innerText = repeatEnabled ? 'Repeat: On' : 'Repeat: Off';
    repeatBtn.classList.toggle('active', repeatEnabled);
}

function toggleRepeat() {
    repeatEnabled = !repeatEnabled;
    updateRepeatButtonUI();

    // Rebuild source node to apply loop mode while preserving position.
    if (playbackState === "playing") {
        const currentTime = audioContext.currentTime - startTime + pauseOffset;
        startPlayback(currentTime % duration);
    }
}

audioUpload.addEventListener('change', handleFileUpload);
playPauseBtn.addEventListener('click', togglePlayPause);
stopBtn.addEventListener('click', stopAudio);
repeatBtn.addEventListener('click', toggleRepeat);
progressBar.addEventListener('input', handleSeek); // 'input' fires while dragging
updateRepeatButtonUI();

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

let currentSelectionMode = 0; // 0 = Authentic, 1 = Combo
let turboEngaged = false;
let turboStateSnapshot = null;
const etaNoisBtn = document.getElementById('eta-nois-btn');

const selModeRadios = document.querySelectorAll('input[name="selection-mode"]');
selModeRadios.forEach(radio => {
    radio.addEventListener('change', (e) => {
        currentSelectionMode = parseInt(e.target.value, 10);
        setSelectionMode(currentSelectionMode);

        // Re-evaluate checkboxes
        if (currentSelectionMode === 0) {
            // Authentic mode: ensure only one is selected
            const checkedBoxes = Array.from(modeCheckboxes).filter(cb => cb.checked);
            if (checkedBoxes.length > 1) {
                // Keep the lowest active one, uncheck the rest
                let first = true;
                checkedBoxes.forEach(cb => {
                    if (first) {
                        first = false;
                        setMode(cb.value);
                    } else {
                        cb.checked = false;
                    }
                });
            } else if (checkedBoxes.length === 1) {
                setMode(checkedBoxes[0].value);
            } else {
                // None selected (shouldn't happen), default to mode 1
                modeCheckboxes[0].checked = true;
                setMode(0);
            }
        } else {
            // Switching to combo: push current mask
            updateMask();
        }
    });
});

const modeCheckboxes = document.querySelectorAll('input[name="mode"]');

function updateMask() {
    let mask = 0;
    modeCheckboxes.forEach(cb => {
        if (cb.checked) {
            mask |= (1 << parseInt(cb.value, 10));
        }
    });

    // Prevent unselecting all (if none, reselect mode 1)
    if (mask === 0) {
        modeCheckboxes[0].checked = true;
        mask = 1;
    }

    setModeMask(mask);
}

modeCheckboxes.forEach(checkbox => {
    checkbox.addEventListener('change', (e) => {
        if (currentSelectionMode === 0) {
            // Authentic mode: enforce radio button behavior
            if (e.target.checked) {
                modeCheckboxes.forEach(cb => {
                    if (cb !== e.target) cb.checked = false;
                });
                setMode(e.target.value);
            } else {
                // Prevent unchecking the last active button
                e.target.checked = true;
            }
        } else {
            // Combo mode: allow multiple, just update mask
            updateMask();
        }
    });
});

document.getElementById('play-btn').addEventListener('click', toggleOscillator);

function engageTurboMode() {
    if (turboEngaged) return;

    turboStateSnapshot = {
        selectionMode: currentSelectionMode,
        checkedModes: Array.from(modeCheckboxes).map(cb => cb.checked)
    };

    turboEngaged = true;
    etaNoisBtn.classList.add('active');

    currentSelectionMode = 1;
    const comboRadio = document.getElementById('sel-combo');
    comboRadio.checked = true;
    setSelectionMode(1);

    modeCheckboxes.forEach(cb => {
        cb.checked = true;
    });
    updateMask();
}

function releaseTurboMode() {
    if (!turboEngaged || !turboStateSnapshot) return;

    currentSelectionMode = turboStateSnapshot.selectionMode;
    const targetSelectionRadioId = currentSelectionMode === 1 ? 'sel-combo' : 'sel-authentic';
    document.getElementById(targetSelectionRadioId).checked = true;
    setSelectionMode(currentSelectionMode);

    modeCheckboxes.forEach((cb, index) => {
        cb.checked = turboStateSnapshot.checkedModes[index];
    });

    if (currentSelectionMode === 1) {
        updateMask();
    } else {
        const checkedBox = Array.from(modeCheckboxes).find(cb => cb.checked) || modeCheckboxes[0];
        checkedBox.checked = true;
        modeCheckboxes.forEach(cb => {
            if (cb !== checkedBox) cb.checked = false;
        });
        setMode(checkedBox.value);
    }

    turboEngaged = false;
    turboStateSnapshot = null;
    etaNoisBtn.classList.remove('active');
}

etaNoisBtn.addEventListener('mousedown', engageTurboMode);
etaNoisBtn.addEventListener('touchstart', (event) => {
    event.preventDefault();
    engageTurboMode();
}, { passive: false });
etaNoisBtn.addEventListener('mouseup', releaseTurboMode);
etaNoisBtn.addEventListener('mouseleave', releaseTurboMode);
etaNoisBtn.addEventListener('touchend', releaseTurboMode);
etaNoisBtn.addEventListener('touchcancel', releaseTurboMode);

// Custom Parameters UI Logic
let baseParamsSnapshot = null;
let isCustomMode = false;
const editedBadge = document.getElementById('edited-badge');
const expertToggle = document.getElementById('expert-toggle');
const expertContent = document.getElementById('expert-content');
const tempPresetsStatus = document.getElementById('temp-presets-status');

expertToggle.addEventListener('click', () => {
    expertToggle.classList.toggle('open');
    expertContent.classList.toggle('open');
});

function markAsEdited() {
    if (!isCustomMode) {
        isCustomMode = true;
        enableCustomParams(true);
        setCustomModeUI(true);
    }
}

function setCustomModeUI(active) {
    if (active) {
        editedBadge.style.display = 'inline-block';
        document.querySelector('.hardware-container').classList.add('custom-mode-active');
    } else {
        editedBadge.style.display = 'none';
        document.querySelector('.hardware-container').classList.remove('custom-mode-active');
    }
}

function resetToPreset() {
    isCustomMode = false;
    enableCustomParams(false);
    setCustomModeUI(false);

    // Determine which mode to reload
    let modeToLoad = 0;
    if (currentSelectionMode === 0) {
        const checkedBox = Array.from(modeCheckboxes).find(cb => cb.checked);
        if (checkedBox) modeToLoad = checkedBox.value;
    }
    loadBaseMode(modeToLoad);
}

document.getElementById('reset-preset-btn').addEventListener('click', resetToPreset);

function captureSoundState() {
    return {
        selectionMode: currentSelectionMode,
        checkedModes: Array.from(modeCheckboxes).map(cb => cb.checked),
        isCustomMode,
        params: currentDSPParams ? { ...currentDSPParams } : null
    };
}

function applySoundState(state) {
    if (!state) return;

    currentSelectionMode = state.selectionMode;
    document.getElementById(currentSelectionMode === 1 ? 'sel-combo' : 'sel-authentic').checked = true;
    setSelectionMode(currentSelectionMode);

    modeCheckboxes.forEach((cb, index) => {
        cb.checked = !!state.checkedModes[index];
    });

    if (currentSelectionMode === 1) {
        updateMask();
    } else {
        const checkedBox = Array.from(modeCheckboxes).find(cb => cb.checked) || modeCheckboxes[0];
        checkedBox.checked = true;
        modeCheckboxes.forEach(cb => {
            if (cb !== checkedBox) cb.checked = false;
        });
        setMode(checkedBox.value);
    }

    isCustomMode = !!state.isCustomMode;
    enableCustomParams(isCustomMode);
    setCustomModeUI(isCustomMode);

    if (state.params) {
        Object.entries(state.params).forEach(([key, value]) => {
            setCustomParam(key, value);
        });
        currentDSPParams = { ...state.params };
        updateUIFromParams(state.params);
    }
}

function updateUIFromParams(params) {
    if (!isCustomMode) {
        // Store as base preset reference when not editing
        baseParamsSnapshot = { ...params };
    }

    // Quick Edit
    document.getElementById('param-rate').value = params.rateHz;
    document.getElementById('val-rate').innerText = params.rateHz.toFixed(2);

    document.getElementById('param-depth').value = params.depthMs;
    document.getElementById('val-depth').innerText = params.depthMs.toFixed(2);

    document.getElementById('param-base').value = params.baseMs;
    document.getElementById('val-base').innerText = params.baseMs.toFixed(2);

    document.getElementById('param-mainwet').value = params.mainWet;
    document.getElementById('val-mainwet').innerText = params.mainWet.toFixed(2);

    document.getElementById('param-crosswet').value = params.crossWet;
    document.getElementById('val-crosswet').innerText = params.crossWet.toFixed(2);

    // Advanced
    document.getElementById('param-offset2').value = params.baseOffset2Ms;
    document.getElementById('val-offset2').innerText = params.baseOffset2Ms.toFixed(2);

    document.getElementById('param-depth2scale').value = params.depth2Scale;
    document.getElementById('val-depth2scale').innerText = params.depth2Scale.toFixed(2);

    // Derived values for Balance
    // bal is mapped from gain diff back to roughly [-1, 1].
    // Our macro sets: gain2 += val * 0.15 (if val < 0), or gain1 -= val * 0.15 (if val > 0)
    let bal = 0;
    const diff = params.wet1Gain - params.wet2Gain;
    if (diff > 0.01) { // gain1 > gain2 -> val was negative
        bal = -diff / 0.15;
    } else if (diff < -0.01) { // gain2 > gain1 -> val was positive
        bal = -diff / 0.15;
    }
    bal = Math.max(-1, Math.min(1, bal));
    document.getElementById('param-balance').value = bal;
    document.getElementById('val-balance').innerText = bal.toFixed(2);

    // Derived value for Tone (approximate mapping from HPF/LPF to 0-100)
    // 0 = Dark (HPF 120, LPF 4000), 100 = Bright (HPF 250, LPF 5600)
    const toneVal = Math.max(0, Math.min(100, (params.lpf1Hz - 4000) / 16));
    document.getElementById('param-tone').value = toneVal;
    document.getElementById('val-tone').innerText = Math.round(toneVal);

    // Expert
    document.getElementById('param-hpf1').value = params.hpf1Hz;
    document.getElementById('val-hpf1').innerText = params.hpf1Hz.toFixed(0);

    document.getElementById('param-lpf1').value = params.lpf1Hz;
    document.getElementById('val-lpf1').innerText = params.lpf1Hz.toFixed(0);

    document.getElementById('param-hpf2').value = params.hpf2Hz;
    document.getElementById('val-hpf2').innerText = params.hpf2Hz.toFixed(0);

    document.getElementById('param-lpf2').value = params.lpf2Hz;
    document.getElementById('val-lpf2').innerText = params.lpf2Hz.toFixed(0);

    document.getElementById('param-gain1').value = params.wet1Gain;
    document.getElementById('val-gain1').innerText = params.wet1Gain.toFixed(2);

    document.getElementById('param-gain2').value = params.wet2Gain;
    document.getElementById('val-gain2').innerText = params.wet2Gain.toFixed(2);
}

// Save / Load JSON
let currentDSPParams = null; // To hold the latest params for saving

// Listen to customParamsUpdate to update currentDSPParams
const originalUpdateUIFromParams = updateUIFromParams;
updateUIFromParams = function(params) {
    currentDSPParams = params;
    originalUpdateUIFromParams(params);
};

document.getElementById('save-json-btn').addEventListener('click', () => {
    if (!currentDSPParams) {
        // If we don't have current params, fetch them first
        getCustomParams();
        setTimeout(() => {
            if (currentDSPParams) triggerSaveJSON();
        }, 100); // Wait a bit for async return
    } else {
        triggerSaveJSON();
    }
});

function triggerSaveJSON() {
    const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(currentDSPParams, null, 2));
    const dlAnchorElem = document.createElement('a');
    dlAnchorElem.setAttribute("href", dataStr);
    dlAnchorElem.setAttribute("download", "dimension_chorus_preset.json");
    dlAnchorElem.click();
}

document.getElementById('load-json-btn').addEventListener('click', () => {
    document.getElementById('load-json-input').click();
});

document.getElementById('load-json-input').addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (event) => {
        try {
            const params = JSON.parse(event.target.result);
            markAsEdited(); // Ensure we are in custom mode

            // Set each param to DSP
            for (const [key, value] of Object.entries(params)) {
                setCustomParam(key, value);
            }

            // Re-fetch to update UI from DSP to ensure bounds are respected
            getCustomParams();
        } catch (err) {
            console.error("Failed to parse JSON preset:", err);
            alert("Invalid JSON preset file.");
        }
    };
    reader.readAsText(file);
    e.target.value = ''; // Reset input
});

const rangesConfig = {
    safe: {
        'param-rate': { min: 0.10, max: 0.80 },
        'param-depth': { min: 0.20, max: 1.50 },
        'param-base': { min: 8.0, max: 16.0 },
        'param-mainwet': { min: 0.08, max: 0.40 },
        'param-crosswet': { min: 0.05, max: 0.30 },
        'param-offset2': { min: 0.00, max: 0.30 },
        'param-depth2scale': { min: 0.80, max: 1.10 },
        'param-hpf1': { min: 120, max: 250 },
        'param-lpf1': { min: 4000, max: 5600 },
        'param-hpf2': { min: 120, max: 250 },
        'param-lpf2': { min: 4000, max: 5600 },
        'param-gain1': { min: 0.88, max: 1.03 },
        'param-gain2': { min: 0.88, max: 1.03 }
    },
    unlocked: {
        'param-rate': { min: 0.01, max: 10.0 },
        'param-depth': { min: 0.0, max: 10.0 },
        'param-base': { min: 1.0, max: 30.0 },
        'param-mainwet': { min: 0.0, max: 1.0 },
        'param-crosswet': { min: 0.0, max: 1.0 },
        'param-offset2': { min: -5.00, max: 5.00 },
        'param-depth2scale': { min: 0.0, max: 2.0 },
        'param-hpf1': { min: 20, max: 1000 },
        'param-lpf1': { min: 200, max: 20000 },
        'param-hpf2': { min: 20, max: 1000 },
        'param-lpf2': { min: 200, max: 20000 },
        'param-gain1': { min: 0.0, max: 2.0 },
        'param-gain2': { min: 0.0, max: 2.0 }
    }
};

const unlockRangesCb = document.getElementById('unlock-ranges-cb');
unlockRangesCb.addEventListener('change', (e) => {
    const mode = e.target.checked ? 'unlocked' : 'safe';
    for (const [id, bounds] of Object.entries(rangesConfig[mode])) {
        const el = document.getElementById(id);
        if (el) {
            // Must cast to string so Playwright / DOM treats it reliably
            el.min = bounds.min.toString();
            el.max = bounds.max.toString();
        }
    }
});

// Temporary A/B preset compare
let tempPresetA = null;
let tempPresetB = null;
let compareBaselineState = null;

const saveTempABtn = document.getElementById('save-temp-a-btn');
const loadTempABtn = document.getElementById('load-temp-a-btn');
const saveTempBBtn = document.getElementById('save-temp-b-btn');
const loadTempBBtn = document.getElementById('load-temp-b-btn');
const returnCurrentBtn = document.getElementById('return-current-btn');

function updateTempPresetStatus(message) {
    tempPresetsStatus.innerText = message;
}

function saveTempPreset(slot) {
    const state = captureSoundState();
    if (!state.params) {
        updateTempPresetStatus('Ainda sem parâmetros ativos para salvar. Toque/edite algo primeiro.');
        return;
    }

    if (slot === 'A') {
        tempPresetA = state;
        loadTempABtn.disabled = false;
    } else {
        tempPresetB = state;
        loadTempBBtn.disabled = false;
    }
    updateTempPresetStatus(`Preset temporário ${slot} salvo.`);
}

function loadTempPreset(slot) {
    const preset = slot === 'A' ? tempPresetA : tempPresetB;
    if (!preset) return;

    if (!compareBaselineState) {
        compareBaselineState = captureSoundState();
    }

    applySoundState(preset);
    returnCurrentBtn.disabled = false;
    updateTempPresetStatus(`Ouvindo Temp ${slot}. Você pode alternar entre A/B sem perder o estado atual.`);
}

function restoreCurrentState() {
    if (!compareBaselineState) return;
    applySoundState(compareBaselineState);
    compareBaselineState = null;
    returnCurrentBtn.disabled = true;
    updateTempPresetStatus('Estado atual restaurado.');
}

saveTempABtn.addEventListener('click', () => saveTempPreset('A'));
saveTempBBtn.addEventListener('click', () => saveTempPreset('B'));
loadTempABtn.addEventListener('click', () => loadTempPreset('A'));
loadTempBBtn.addEventListener('click', () => loadTempPreset('B'));
returnCurrentBtn.addEventListener('click', restoreCurrentState);


// Bind Sliders to WASM setters
function bindSlider(id, paramName, formatter = (v) => v.toFixed(2)) {
    const slider = document.getElementById(id);
    const label = document.getElementById(id.replace('param-', 'val-'));
    slider.addEventListener('input', (e) => {
        markAsEdited();
        label.innerText = formatter(parseFloat(e.target.value));
        setCustomParam(paramName, e.target.value);
    });
}

bindSlider('param-rate', 'rateHz');
bindSlider('param-depth', 'depthMs');
bindSlider('param-base', 'baseMs');
bindSlider('param-mainwet', 'mainWet');
bindSlider('param-crosswet', 'crossWet');
bindSlider('param-offset2', 'baseOffset2Ms');
bindSlider('param-depth2scale', 'depth2Scale');

bindSlider('param-hpf1', 'hpf1Hz', v => v.toFixed(0));
bindSlider('param-lpf1', 'lpf1Hz', v => v.toFixed(0));
bindSlider('param-hpf2', 'hpf2Hz', v => v.toFixed(0));
bindSlider('param-lpf2', 'lpf2Hz', v => v.toFixed(0));
bindSlider('param-gain1', 'wet1Gain');
bindSlider('param-gain2', 'wet2Gain');

// Macros
const toneSlider = document.getElementById('param-tone');
const toneLabel = document.getElementById('val-tone');
toneSlider.addEventListener('input', (e) => {
    markAsEdited();
    const val = parseFloat(e.target.value); // 0 to 100
    toneLabel.innerText = Math.round(val);

    // Map 0-100 to HPF 120-250 and LPF 4000-5600
    const hpf = 120 + (val / 100) * 130;
    const lpf = 4000 + (val / 100) * 1600;

    setCustomParam('hpf1Hz', hpf);
    setCustomParam('lpf1Hz', lpf);
    setCustomParam('hpf2Hz', hpf);
    setCustomParam('lpf2Hz', lpf);

    // Update raw sliders visually if expert is open
    document.getElementById('param-hpf1').value = hpf;
    document.getElementById('val-hpf1').innerText = hpf.toFixed(0);
    document.getElementById('param-lpf1').value = lpf;
    document.getElementById('val-lpf1').innerText = lpf.toFixed(0);
    document.getElementById('param-hpf2').value = hpf;
    document.getElementById('val-hpf2').innerText = hpf.toFixed(0);
    document.getElementById('param-lpf2').value = lpf;
    document.getElementById('val-lpf2').innerText = lpf.toFixed(0);
});

const balanceSlider = document.getElementById('param-balance');
const balanceLabel = document.getElementById('val-balance');
balanceSlider.addEventListener('input', (e) => {
    markAsEdited();
    const val = parseFloat(e.target.value); // -1 to +1
    balanceLabel.innerText = val.toFixed(2);

    // Base gain is roughly 0.95-1.0
    const baseGain = 1.0;
    let gain1 = baseGain;
    let gain2 = baseGain;

    if (val < 0) {
        gain2 += val * 0.15; // val is negative, so gain2 goes down
    } else {
        gain1 -= val * 0.15; // val is positive, so gain1 goes down
    }

    setCustomParam('wet1Gain', gain1);
    setCustomParam('wet2Gain', gain2);

    document.getElementById('param-gain1').value = gain1;
    document.getElementById('val-gain1').innerText = gain1.toFixed(2);
    document.getElementById('param-gain2').value = gain2;
    document.getElementById('val-gain2').innerText = gain2.toFixed(2);
});

// Link Voices Logic
const linkVoicesCb = document.getElementById('link-voices-cb');
['hpf1', 'lpf1', 'gain1'].forEach(suffix => {
    document.getElementById(`param-${suffix}`).addEventListener('input', (e) => {
        if (linkVoicesCb.checked) {
            const val = e.target.value;
            const targetSuffix = suffix.replace('1', '2');
            const targetName = suffix.replace('1', '2Hz').replace('gain1', 'wet2Gain');

            document.getElementById(`param-${targetSuffix}`).value = val;
            const numericVal = parseFloat(val);
            const formattedVal = suffix.startsWith('gain') ? numericVal.toFixed(2) : numericVal.toFixed(0);
            document.getElementById(`val-${targetSuffix}`).innerText = formattedVal;

            // Re-fire for voice 2
            setCustomParam(targetName, val);
        }
    });
});

// Update mode selection to handle preset loading
modeCheckboxes.forEach(checkbox => {
    checkbox.addEventListener('change', (e) => {
        if (currentSelectionMode === 0) {
            if (e.target.checked) {
                // ... (existing logic in main.js will run) ...
                if (!isCustomMode) {
                    loadBaseMode(e.target.value);
                }
            }
        }
    });
});

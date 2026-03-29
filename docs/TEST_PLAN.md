# TEST PLAN

## Offline DSP
- Impulse
- Sine 440 Hz
- 1 kHz headroom
- Log sweep
- White/pink noise
- Mode switching
- Mono sum

## Hardware Bring-up
- Passthrough validation
- 24-in-32 alignment
- DMA half/full callback integrity
- D-cache / MPU validation
- CPU time budget with GPIO toggle

## Acceptance
- No obvious clicks in normal operation
- No block overruns
- Stable stereo output
- All 4 modes render and run
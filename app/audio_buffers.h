#ifndef AUDIO_BUFFERS_H
#define AUDIO_BUFFERS_H

/**
 * @file audio_buffers.h
 * @brief Common explicit definitions for audio configuration (Sample Rate, Block Size, etc.)
 */

#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_BLOCK_FRAMES 32
#define AUDIO_CHANNELS 2 // Stereo output
#define AUDIO_DMA_BUFFER_FRAMES (AUDIO_BLOCK_FRAMES * 2) // Double buffering (Ping-Pong)
#define AUDIO_DMA_WORDS (AUDIO_DMA_BUFFER_FRAMES * AUDIO_CHANNELS)

#endif // AUDIO_BUFFERS_H

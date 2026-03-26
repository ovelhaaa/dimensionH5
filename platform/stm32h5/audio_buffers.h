#ifndef AUDIO_BUFFERS_H
#define AUDIO_BUFFERS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file audio_buffers.h
 * @brief Memory containers for DMA and Audio processing.
 *
 * This file defines the explicit sizes and declarations of audio buffers used by
 * the DMA (SAI/I2S) to interface with the STM32H5.
 *
 * Architecture Assumptions:
 * - 48 kHz Sample Rate
 * - Stereo interleaved RX/TX via I2S or SAI
 * - Processing in half-blocks of 32 frames (64 interleaved int32_t words)
 * - The full DMA buffer size is 64 frames (128 interleaved int32_t words)
 */

#define AUDIO_BLOCK_FRAMES 32
#define AUDIO_CHANNELS 2

/**
 * A single processing block contains 32 frames * 2 channels = 64 words.
 */
#define AUDIO_BLOCK_WORDS (AUDIO_BLOCK_FRAMES * AUDIO_CHANNELS)

/**
 * The full DMA buffer has 2 halves (Ping-Pong).
 * 64 frames * 2 channels = 128 words.
 */
#define AUDIO_DMA_BUFFER_FRAMES (AUDIO_BLOCK_FRAMES * 2)
#define AUDIO_DMA_BUFFER_WORDS  (AUDIO_DMA_BUFFER_FRAMES * AUDIO_CHANNELS)

/*
 * DMA Buffers (Intended to be mapped to SRAM sections accessible by DMA)
 * We assume the container is int32_t.
 */
extern int32_t dma_rx_buffer[AUDIO_DMA_BUFFER_WORDS];
extern int32_t dma_tx_buffer[AUDIO_DMA_BUFFER_WORDS];

#endif // AUDIO_BUFFERS_H

#include "audio_buffers.h"

/**
 * @file audio_buffers.c
 * @brief Allocation of the DMA Audio buffers.
 *
 * Note for future optimization / linkage:
 * On STM32H5 architectures, these arrays should ideally be placed in memory regions
 * explicitly configured for DMA access without caching issues, or handled with
 * explicit cache invalidate/clean operations (if Data Cache is enabled).
 * You can do this later using linker section attributes, e.g.:
 * __attribute__((section(".dma_ram"))) int32_t dma_rx_buffer[...];
 */

int32_t dma_rx_buffer[AUDIO_FULL_BUFFER_WORDS];
int32_t dma_tx_buffer[AUDIO_FULL_BUFFER_WORDS];

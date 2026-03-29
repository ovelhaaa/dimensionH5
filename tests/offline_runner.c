#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "core/dimension_chorus.h"

// Um gerador pseudo-aleatório simples para o runner offline
// em caso de não termos um arquivo WAV disponivel. O objetivo
// deste runner agora é simular o laço contínuo do hardware e gerar um log.

#define SAMPLE_RATE 48000
#define BLOCK_SIZE 32
#define TOTAL_BLOCKS 100 // Processar equivalente a pequenos ms

int main() {
    printf("Starting Offline Runner...\n");

    DimensionChorusState chorus;
    DimensionChorus_Init(&chorus);
    DimensionChorus_SetMode(&chorus, 0); // Default mode

    float inMono[BLOCK_SIZE] = {0.0f};
    float outStereo[BLOCK_SIZE * 2] = {0.0f};

    // Gerador de noise de baixa amplitude pra simular input
    for (int i = 0; i < BLOCK_SIZE; i++) {
        inMono[i] = ((float)rand() / RAND_MAX) * 0.1f;
    }

    printf("Processing %d blocks of %d frames...\n", TOTAL_BLOCKS, BLOCK_SIZE);
    for (int b = 0; b < TOTAL_BLOCKS; b++) {
        DimensionChorus_ProcessBlock(&chorus, inMono, outStereo, BLOCK_SIZE);

        // Simular a mudança de modo dinamicamente no meio do teste
        if (b == 50) {
            printf("Switching mode at block %d...\n", b);
            DimensionChorus_SetMode(&chorus, 2);
        }
    }

    printf("Offline Runner Completed.\n");

    // Aqui no futuro adicionaremos o código da lib dr_wav para gravar "outL/outR" em out.wav.
    // O foco agora é ter a base de testes automatizável no pipeline CMake.

    return 0;
}

#ifndef BUTTERWORTH_FILTER_H
#define BUTTERWORTH_FILTER_H

typedef struct {
    float *x1, *y1; // Buffers de atraso para a seção 1
    float *x2, *y2; // Buffers de atraso para a seção 2
} ButterworthFilter;

// Inicializa o filtro com memória alocada para os buffers
void butterworth_init(ButterworthFilter *filter);

// Aplica o filtro Butterworth de 2ª ordem aos dados de entrada
void butterworth_apply(ButterworthFilter *filter, float *input, float *output, int num_samples);

// Libera a memória alocada para o filtro
void butterworth_free(ButterworthFilter *filter);

#endif // BUTTERWORTH_FILTER_H

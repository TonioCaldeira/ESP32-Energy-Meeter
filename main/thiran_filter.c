#include "thiran_filter.h"

// Função para inicializar o filtro Thiran, zerando o estado
void thiran_init(ThiranFilter *filter) {
    filter->prev_input = 0.0f;
    filter->prev_output = 0.0f;
}

// Função para aplicar o filtro Thiran em um conjunto de amostras
void thiran_apply(ThiranFilter *filter, float *input, float *output, int num_samples) {
    const float b0 = 0.7094f;
    const float b1 = 1.0f;
    const float a1 = 0.7094f;

    for (int i = 0; i < num_samples; i++) {
        // Aplica o filtro Thiran em cada amostra
        output[i] = b0 * input[i] + b1 * filter->prev_input - a1 * filter->prev_output;

        // Atualiza o estado do filtro
        filter->prev_input = input[i];
        filter->prev_output = output[i];
    }
}

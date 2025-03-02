#ifndef THIRAN_FILTER_H
#define THIRAN_FILTER_H

// Estrutura do filtro Thiran para armazenar o estado
typedef struct {
    float prev_input;
    float prev_output;
} ThiranFilter;

// Função para inicializar o filtro Thiran
void thiran_init(ThiranFilter *filter);

// Função para aplicar o filtro Thiran em um conjunto de amostras
void thiran_apply(ThiranFilter *filter, float *input, float *output, int num_samples);

#endif // THIRAN_FILTER_H

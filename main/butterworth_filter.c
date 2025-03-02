#include "butterworth_filter.h"
#include <stdlib.h>
#include "esp_log.h"

#define TAG "FILTER"

// Coeficientes do filtro Butterworth de 2ª ordem, com duas seções
// Esses coeficientes correspondem a Fc = 350Hz e Fs = 4800Hz
static const float b1[3] = {1.0f, 2.0f, 1.0f};
static const float a1[3] = {1.0f, -1.534090595278471f, 0.710488594688278f};
static const float gain1 = 0.0440994998524517f;

static const float b2[3] = {1.0f, 2.0f, 1.0f};
static const float a2[3] = {1.0f, -1.273404902142154f, 0.419827856476049f};
static const float gain2 = 0.0366057385834737f;

static const float output_gain = 1.0f;

void butterworth_init(ButterworthFilter *filter) {
    // Alocar memória para os buffers das duas seções
    filter->x1 = (float *)calloc(2, sizeof(float));
    filter->y1 = (float *)calloc(2, sizeof(float));
    filter->x2 = (float *)calloc(2, sizeof(float));
    filter->y2 = (float *)calloc(2, sizeof(float));
}

void butterworth_apply(ButterworthFilter *filter, float *input, float *output, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        // Seção 1
        float new_y1 = gain1 * (b1[0] * input[i] + b1[1] * filter->x1[0] + b1[2] * filter->x1[1]);
        new_y1 -= (a1[1] * filter->y1[0] + a1[2] * filter->y1[1]);

        // Atualizar os buffers da seção 1
        filter->x1[1] = filter->x1[0];
        filter->x1[0] = input[i];
        filter->y1[1] = filter->y1[0];
        filter->y1[0] = new_y1;

        // Seção 2
        float new_y2 = gain2 * (b2[0] * new_y1 + b2[1] * filter->x2[0] + b2[2] * filter->x2[1]);
        new_y2 -= (a2[1] * filter->y2[0] + a2[2] * filter->y2[1]);

        // Atualizar os buffers da seção 2
        filter->x2[1] = filter->x2[0];
        filter->x2[0] = new_y1;
        filter->y2[1] = filter->y2[0];
        filter->y2[0] = new_y2;

        // Aplicar o ganho de saída
        output[i] = new_y2 * output_gain;
    }
}

void butterworth_free(ButterworthFilter *filter) {
    // Liberar memória alocada
    free(filter->x1);
    free(filter->y1);
    free(filter->x2);
    free(filter->y2);
}

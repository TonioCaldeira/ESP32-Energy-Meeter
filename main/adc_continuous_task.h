#ifndef ADC_CONTINUOUS_TASK_H
#define ADC_CONTINUOUS_TASK_H

#include "esp_adc/adc_continuous.h"
#include "config.h"
extern TaskHandle_t udp_cast_task_handle;


//extern int sampling_rate;       // Taxa de amostragem (amostras por segundo por canal)
//extern int samples_per_packet;  // Número de amostras por canal dentro do pacote

typedef struct {
    int packet_count;
    short error_flag;
    short active_channels;
    int sample_rate;
    float UDP_rate_real;
    short calib_coeff_atten;
    short calib_dc_offset;
    short samples_per_channel;
    short calib_coeff_a;
    short calib_coeff_b;
    short coeff_channel_0;
    short coeff_channel_1;
    short coeff_channel_2;
    short coeff_channel_3;
    short coeff_channel_4;
    short coeff_channel_5;
    short samples[MAX_CHANNELS][SAMPLES_PER_CHANNEL];
} DataPacket;

void adc_continuous_task(void *pvParameters);
void end_adc_continuous_task();
void start_adc_continuous_task();

extern DataPacket data_packet;

#endif // ADC_CONTINUOUS_TASK_H

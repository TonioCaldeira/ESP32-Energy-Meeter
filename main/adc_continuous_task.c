#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"
#include "adc_continuous_task.h"
#include "esp_timer.h"
#include "butterworth_filter.h"
#include "thiran_filter.h"

#define TAG "ADC_CONTINUOUS"

// Taxa de amostragem global para atingir 4800 amostras/s por canal
const int sampling_rate = SPS * MAX_CHANNELS;
const int samples_per_packet = SAMPLES_PER_CHANNEL;

DataPacket data_packet;
static int packet_count = 0;

static TaskHandle_t s_task_handle;

/**
 * @brief Callback executado quando a conversão ADC é concluída.
 *
 * Notifica a tarefa que aguarda a finalização da conversão.
 */
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle,
                                     const adc_continuous_evt_data_t *edata,
                                     void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
    return (mustYield == pdTRUE) ? pdTRUE : pdFALSE;
}

/**
 * @brief Inicializa o ADC contínuo.
 *
 * Configura os parâmetros do ADC, define os canais e prepara o handle para
 * conversões contínuas.
 *
 * @param handle Ponteiro para o handle do ADC contínuo.
 * @return esp_err_t Código de erro (ESP_OK em caso de sucesso).
 */
static esp_err_t continuous_adc_init(adc_continuous_handle_t *handle)
{
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = MAX_CHANNELS * samples_per_packet * SOC_ADC_DIGI_RESULT_BYTES * 4,
        .conv_frame_size    = MAX_CHANNELS * samples_per_packet * SOC_ADC_DIGI_RESULT_BYTES,
    };

    esp_err_t ret = adc_continuous_new_handle(&adc_config, handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC continuous handle: %s", esp_err_to_name(ret));
        return ret;
    }

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = sampling_rate,    // Configura a taxa de amostragem global
        .conv_mode      = ADC_CONV_SINGLE_UNIT_1,
        .format         = ADC_OUTPUT_TYPE,
    };

    // Definindo os canais de acordo com a plataforma (ESP32 ou ESP32S3)
#if CONFIG_IDF_TARGET_ESP32
    static adc_channel_t channels[MAX_CHANNELS] = {
        ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_6,
        ADC_CHANNEL_7, ADC_CHANNEL_4, ADC_CHANNEL_5
    };
#else
    static adc_channel_t channels[MAX_CHANNELS] = {
        ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5,
        ADC_CHANNEL_6, ADC_CHANNEL_8, ADC_CHANNEL_9
    };
#endif

    adc_digi_pattern_config_t adc_pattern[MAX_CHANNELS] = {0};

    dig_cfg.pattern_num = MAX_CHANNELS;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        adc_pattern[i].atten     = COEFF_ATTEN;
        adc_pattern[i].channel   = channels[i] & 0x7;
        adc_pattern[i].unit      = ADC_UNIT_1;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }
    data_packet.UDP_rate_real = 0;
    dig_cfg.adc_pattern = adc_pattern;

    ret = adc_continuous_config(*handle, &dig_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC continuous: %s", esp_err_to_name(ret));
    }

    return ret;
}

// Filtros Butterworth para cada canal
ButterworthFilter butt_filters[MAX_CHANNELS];
static bool butt_filter_initialized[MAX_CHANNELS] = { false };

/**
 * @brief Aplica o filtro Butterworth aos dados de um canal.
 *
 * Inicializa o filtro, converte os dados para float, aplica o filtro e
 * converte os dados filtrados de volta para o formato original.
 *
 * @param data_packet Ponteiro para o pacote de dados.
 * @param channel Canal a ser filtrado.
 */
void apply_butterworth_filter(DataPacket *data_packet, int channel)
{
    // Inicializa o filtro para o canal, se necessário
    if (!butt_filter_initialized[channel]) {
        butterworth_init(&butt_filters[channel]);
        butt_filter_initialized[channel] = true;
    }

    int num_samples = data_packet->samples_per_channel;
    float *input = (float *)malloc(num_samples * sizeof(float));
    float *output = (float *)malloc(num_samples * sizeof(float));

    if (input == NULL || output == NULL) {
        // Trata erro de alocação de memória
        free(input);
        free(output);
        return;
    }

    // Converte os dados para float
    for (int i = 0; i < num_samples; i++) {
        input[i] = (float)data_packet->samples[channel][i];
    }

    // Aplica o filtro Butterworth
    butterworth_apply(&butt_filters[channel], input, output, num_samples);

    // Converte os dados filtrados de volta para short, limitando os valores
    for (int i = 0; i < num_samples; i++) {
        if (output[i] > SHRT_MAX)
            output[i] = SHRT_MAX;
        else if (output[i] < SHRT_MIN)
            output[i] = SHRT_MIN;
        data_packet->samples[channel][i] = (short)output[i];
    }

    free(input);
    free(output);
}

// Filtros Thiran para cada canal
ThiranFilter thiran_filters[MAX_CHANNELS];
static bool thiran_filter_initialized[MAX_CHANNELS] = { false };

/**
 * @brief Aplica o filtro Thiran aos dados de um canal.
 *
 * Inicializa o filtro, converte os dados para float, aplica o filtro e
 * converte os dados filtrados de volta para o formato original.
 *
 * @param data_packet Ponteiro para o pacote de dados.
 * @param channel Canal a ser filtrado.
 */
void apply_thiran_filter(DataPacket *data_packet, int channel)
{
    // Inicializa o filtro para o canal, se necessário
    if (!thiran_filter_initialized[channel]) {
        thiran_init(&thiran_filters[channel]);
        thiran_filter_initialized[channel] = true;
    }

    int num_samples = data_packet->samples_per_channel;
    float *input = (float *)malloc(num_samples * sizeof(float));
    float *output = (float *)malloc(num_samples * sizeof(float));

    if (input == NULL || output == NULL) {
        // Trata erro de alocação de memória
        free(input);
        free(output);
        return;
    }

    // Converte os dados para float
    for (int i = 0; i < num_samples; i++) {
        input[i] = (float)data_packet->samples[channel][i];
    }

    // Aplica o filtro Thiran
    thiran_apply(&thiran_filters[channel], input, output, num_samples);

    // Converte os dados filtrados de volta para short, limitando os valores
    for (int i = 0; i < num_samples; i++) {
        if (output[i] > SHRT_MAX)
            output[i] = SHRT_MAX;
        else if (output[i] < SHRT_MIN)
            output[i] = SHRT_MIN;
        data_packet->samples[channel][i] = (short)output[i];
    }

    free(input);
    free(output);
}

/**
 * @brief Processa os dados de amostragem do ADC e atualiza o pacote de dados.
 *
 * Esta função processa os dados lidos do ADC, preenche os canais do pacote,
 * aplica filtros (se habilitados) e calcula a taxa de amostragem real.
 *
 * @param result Buffer contendo os dados lidos do ADC.
 * @param ret_num Número de bytes lidos.
 * @param channels Vetor com os canais utilizados.
 */
static void process_adc_data(uint8_t *result, uint32_t ret_num, adc_channel_t *channels)
{
    gpio_set_level(GPIO_NUM_21, 1);

    static int sample_index[MAX_CHANNELS] = { 0 };
    static int64_t last_time = 0;
    static int total_samples = 0;
    
    // Limpa o pacote de dados para novos valores
    memset(&data_packet, 0, sizeof(data_packet));

    // Processa os dados de amostragem
    for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
        adc_digi_output_data_t *p_data = (adc_digi_output_data_t *)&result[i];
        int channel_value = ADC_CHANNEL;
        int data = ADC_DATA;

        // Encontra o índice do canal correspondente
        int channel_index = -1;
        for (int j = 0; j < MAX_CHANNELS; j++) {
            if (channels[j] == channel_value) {
                channel_index = j;
                break;
            }
        }

        // Se o canal for válido e ainda houver espaço no pacote, armazena o dado
        if (channel_index >= 0 && sample_index[channel_index] < samples_per_packet) {
            data_packet.samples[channel_index][sample_index[channel_index]] = data;
            sample_index[channel_index]++;
            total_samples++;
        }
    }

    // Preenche os dados faltantes para cada canal
    for (int ch = 0; ch < MAX_CHANNELS; ch++) {
        for (int aux = sample_index[ch]; aux < samples_per_packet; aux++) {
            data_packet.samples[ch][sample_index[ch] + 1] =
                data_packet.samples[ch][sample_index[ch]];
            sample_index[ch]++;
            total_samples++;
        }
        if (data_packet.samples[ch][0] == 0) {
            data_packet.samples[ch][0] = data_packet.samples[ch][1];
        }
        if (data_packet.samples[ch][samples_per_packet - 1] == 0) {
            data_packet.samples[ch][samples_per_packet - 1] =
                data_packet.samples[ch][samples_per_packet - 2];
        }
    }

    // Atualiza os metadados do pacote de dados
    data_packet.packet_count      = packet_count++;
    data_packet.error_flag        = 0;
    data_packet.active_channels   = MAX_CHANNELS;
    data_packet.sample_rate       = sampling_rate / MAX_CHANNELS; // Amostragem por canal
    data_packet.calib_coeff_atten = COEFF_ATTEN;    // Coeficiente de atenuação
    data_packet.calib_dc_offset   = DC_OffSet;      // Coeficiente de offset DC
    data_packet.samples_per_channel = samples_per_packet;
    data_packet.coeff_channel_0   = COEFF_CH_1;
    data_packet.coeff_channel_1   = COEFF_CH_2;
    data_packet.coeff_channel_2   = COEFF_CH_3;
    data_packet.coeff_channel_3   = COEFF_CH_4;
    data_packet.coeff_channel_4   = COEFF_CH_5;
    data_packet.coeff_channel_5   = COEFF_CH_6;
    data_packet.calib_coeff_a     = COEFF_ADC_A;
    data_packet.calib_coeff_b     = COEFF_ADC_B;

    gpio_set_level(GPIO_NUM_21, 0);

    // Aplica os filtros, se estiverem habilitados
    if (APPLYTHIRANFILTER) {
        apply_thiran_filter(&data_packet, 1);
        apply_thiran_filter(&data_packet, 3);
        apply_thiran_filter(&data_packet, 5);
    }
    if (APPLYBUTTERWORTHFILTER) {
        apply_butterworth_filter(&data_packet, 0);
        apply_butterworth_filter(&data_packet, 1);
        apply_butterworth_filter(&data_packet, 2);
        apply_butterworth_filter(&data_packet, 3);
        apply_butterworth_filter(&data_packet, 4);
        apply_butterworth_filter(&data_packet, 5);
    }
    gpio_set_level(GPIO_NUM_21, 1);

    // Calcula a taxa de amostragem real com base no tempo decorrido
    int64_t current_time = esp_timer_get_time(); // Tempo atual em microssegundos
    if (last_time == 0) {
        last_time = current_time;
    }
    int64_t elapsed_time = current_time - last_time; // Tempo decorrido
    if (elapsed_time > 0) {
        float elapsed_s = elapsed_time / 1e6; // Converte para segundos
        data_packet.UDP_rate_real = 1 / elapsed_s;
        last_time = current_time; // Atualiza o tempo para o próximo cálculo
        total_samples = 0;        // Reinicia o contador de amostras
    }

    // Notifica a tarefa de transmissão (udp_cast_task_handle)
    xTaskNotifyGive(udp_cast_task_handle);

    // Reinicia os índices de amostragem para o próximo processamento
    for (int i = 0; i < MAX_CHANNELS; i++) {
        sample_index[i] = 0;
    }
    gpio_set_level(GPIO_NUM_21, 0);
}

/**
 * @brief Tarefa responsável pela coleta contínua de dados do ADC.
 *
 * Inicializa o ADC contínuo, registra callbacks e realiza a leitura dos dados,
 * encaminhando-os para processamento.
 *
 * @param pvParameters Parâmetros passados para a tarefa (não utilizados).
 */
void adc_continuous_task(void *pvParameters)
{
    // Define os canais utilizados para o ESP32
    adc_channel_t channels[MAX_CHANNELS] = {
        ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_6,
        ADC_CHANNEL_7, ADC_CHANNEL_4, ADC_CHANNEL_5
    };

    s_task_handle = xTaskGetCurrentTaskHandle(); // Obtém o handle da tarefa atual

    adc_continuous_handle_t handle;
    if (continuous_adc_init(&handle) != ESP_OK) {
        vTaskDelete(NULL);
        return;
    }

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    uint8_t result[MAX_CHANNELS * samples_per_packet * SOC_ADC_DIGI_RESULT_BYTES];
    uint32_t ret_num = 0;

    while (1) {
        // Aguarda a notificação do callback de conversão
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        esp_err_t ret = adc_continuous_read(handle, result, sizeof(result), &ret_num, 0);
        if (ret == ESP_OK) {
            process_adc_data(result, ret_num, channels);
            // ESP_LOGE(TAG, "spr real: %d", data_packet.sample_rate); // Descomentar para depuração
        } else {
            ESP_LOGE(TAG, "Error reading from ADC: %s", esp_err_to_name(ret));
        }
    }

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
    vTaskDelete(NULL);
}

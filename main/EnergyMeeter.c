#include <stdio.h>
#include "driver/gpio.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "udp_cast_task.h"
#include "adc_continuous_task.h"
#include "wifi_connect.h"
#include "com_task.h"

#define TAG "MAIN" // Define uma tag para logs

void app_main(void) {

    gpio_set_direction(GPIO_NUM_21, GPIO_MODE_OUTPUT);
    // Inicializa o NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret); // Verifica se o NVS foi inicializado corretamente.

    // Inicializa o TCP/IP e cria a rede
    ESP_ERROR_CHECK(esp_netif_init()); // Configura a interface de rede para o ESP32.
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Cria o loop de eventos padrão.

    // Inicializa Wi-Fi
    if (wifi_connect() == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi Connected"); // Mensagem de sucesso na conexão.
    } else {
        ESP_LOGE(TAG, "Error during Wi-Fi connection"); // Mensagem de erro na conexão.
    }

    // Criação da tarefa de comunicação ESP32
    if (xTaskCreate(esp_com_task, "esp_com_task", 4096, NULL, configMAX_PRIORITIES - 10, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error during COM task creation"); // Caso a criação da tarefa falhe.
    }
    
    ESP_LOGI(TAG, "Tasks created successfully."); // Log para indicar que todas as tarefas foram criadas com sucesso.

}

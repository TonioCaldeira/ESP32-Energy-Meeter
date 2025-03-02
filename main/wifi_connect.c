#include "wifi_connect.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "config.h"

#define WIFI_CONNECTED_BIT BIT0    // Indica que o dispositivo está conectado ao Wi-Fi
#define WIFI_FAIL_BIT      BIT1    // Indica falha na conexão após o número máximo de tentativas

// Grupo de eventos para sincronizar a conexão Wi-Fi
static EventGroupHandle_t s_wifi_event_group = NULL;

// Contador de tentativas de reconexão
static int s_retry_num = 0;

// Tag para identificação dos logs
static const char *TAG = "WIFI_connect";

/**
 * @brief Handler para eventos de Wi-Fi e IP.
 *
 * Trata os eventos relacionados à conexão Wi-Fi e à obtenção de endereço IP.
 *
 * @param arg Dados do usuário (não utilizados).
 * @param event_base Base do evento.
 * @param event_id Identificador do evento.
 * @param event_data Dados específicos do evento.
 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Received event: Base=%s, ID=%ld", event_base, event_id);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi iniciado, conectando...");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_WIFI_MAXIMUM_RETRY) {
            ESP_LOGI(TAG, "Tentando reconectar ao Wi-Fi...");
            esp_wifi_connect();
            s_retry_num++;
        }
        else {
            ESP_LOGW(TAG, "Falha ao conectar após várias tentativas.");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Conectado ao Wi-Fi, aguardando IP...");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP obtido: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;  // Reseta o contador de tentativas após obter IP
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else {
        ESP_LOGW(TAG, "Evento não tratado: Base=%s, ID=%ld", event_base, event_id);
    }
}

/**
 * @brief Inicializa e conecta o dispositivo à rede Wi-Fi.
 *
 * Cria a interface de estação, inicializa o Wi-Fi, registra os handlers de eventos,
 * configura a rede e aguarda a conexão ou falha.
 *
 * @return esp_err_t ESP_OK se a conexão for bem-sucedida; caso contrário, ESP_FAIL.
 */
esp_err_t wifi_connect()
{
    // Cria o grupo de eventos para sincronizar a conexão
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Falha ao criar grupo de eventos");
        return ESP_FAIL;
    }

    // Cria a interface padrão para estação (STA)
    esp_netif_create_default_wifi_sta();

    // Inicializa a stack Wi-Fi com a configuração padrão
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registra os handlers para eventos de Wi-Fi e IP
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Configuração da rede Wi-Fi (SSID e senha definidos via menuconfig)
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };

    // Configura o dispositivo como estação (STA)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Inicialização do Wi-Fi concluída.");
    ESP_LOGI(TAG, "Aguardando conexão Wi-Fi...");

    // Aguarda até que a conexão seja estabelecida ou ocorra uma falha
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,  // Não limpa os bits após o retorno
                                           pdFALSE,  // Espera qualquer um dos bits
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado ao SSID:%s, senha:%s", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
        return ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Falha ao conectar ao SSID:%s, senha:%s", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
        return ESP_FAIL;
    }
    else {
        ESP_LOGE(TAG, "Evento inesperado");
        return ESP_FAIL;
    }
}

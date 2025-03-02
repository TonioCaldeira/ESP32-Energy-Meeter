#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "esp_log.h"
#include "lwip/sockets.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "udp_cast_task.h"
#include "adc_continuous_task.h"
#include "config.h"

static const char *TAG = "COM_TASK"; // Tag para logs da tarefa de comunicação

// Armazena o IP do PC selecionado para comunicação unicast
static char selected_ip[16] = "";
static char selected_pc_ip[16] = "";

// Handle da tarefa de broadcast, usado para gerenciar sua exclusão
static TaskHandle_t cast_task_handle = NULL;

// Protótipo da função que inicia a comunicação unicast com o PC
void start_communication_with_pc(void);

/**
 * @brief Tarefa que escuta e processa a escolha do PC para comunicação.
 *
 * Esta tarefa cria um socket UDP para receber mensagens que indicam
 * qual PC selecionou o ESP32 para comunicação. Ao receber a mensagem
 * de seleção, extrai o IP informado, atualiza a variável global COM_IP,
 * inicia a comunicação unicast com o PC e encerra a tarefa de broadcast.
 *
 * @param pvParameters Parâmetros da tarefa (não utilizados).
 */
void listen_for_choice_task(void *pvParameters)
{
    // Cria um socket UDP para escutar mensagens de escolha do PC
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket for choice: errno %d", errno);
        vTaskDelete(NULL);
    }

    // Configura o endereço local para o socket (escuta em qualquer IP na porta definida)
    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(CHOICE_PORT);

    // Associa o socket ao endereço local
    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        ESP_LOGE(TAG, "Unable to bind socket to choice port: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
    }

    char buffer[128];                 // Buffer para receber mensagens
    struct sockaddr_in from_addr;     // Endereço do remetente
    socklen_t from_len = sizeof(from_addr);

    while (1) {
        // Recebe dados via UDP
        int len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from_addr, &from_len);
        if (len < 0) {
            ESP_LOGE(TAG, "Failed to receive data: errno %d", errno);
            continue;
        }

        buffer[len] = '\0'; // Garante que a string esteja terminada em '\0'
        ESP_LOGI(TAG, "Message received: %s", buffer);

        // Verifica se a mensagem começa com "SELECTED"
        if (strncmp(buffer, "SELECTED", 8) == 0) {
            // Armazena o IP do PC que selecionou o ESP32
            inet_ntoa_r(from_addr.sin_addr, selected_pc_ip, sizeof(selected_pc_ip));

            // Extrai o IP informado na mensagem (após "SELECTED ")
            const char *ip_start = buffer + 9;
            strncpy(selected_ip, ip_start, sizeof(selected_ip) - 1);
            selected_ip[sizeof(selected_ip) - 1] = '\0';

            ESP_LOGI(TAG, "ESP32 has been selected by PC: %s (Selected IP: %s)", 
                     selected_pc_ip, selected_ip);

            // Atualiza a variável global COM_IP com o IP do PC
            strncpy(COM_IP, selected_ip, sizeof(COM_IP) - 1);
            COM_IP[sizeof(COM_IP) - 1] = '\0';
            ESP_LOGI(TAG, "COM_IP updated to: %s", COM_IP);

            // Indica que o dispositivo foi selecionado (variável global 'selected')
            selected = 1;

            // Inicia comunicação unicast com o PC
            start_communication_with_pc();

            // Exclui a tarefa de broadcast para economizar recursos
            if (cast_task_handle != NULL) {
                vTaskDelete(cast_task_handle);
                cast_task_handle = NULL;
            }

            break; // Sai do loop após a seleção
        }
    }

    // Fecha o socket e finaliza a tarefa
    close(sock);
    vTaskDelete(NULL);
}

/**
 * @brief Inicia a comunicação unicast com o PC selecionado.
 *
 * Cria um socket UDP e envia uma mensagem de saudação para o PC cuja
 * comunicação foi estabelecida, reiniciando em seguida as tarefas de ADC
 * e transmissão de dados.
 */
void start_communication_with_pc()
{
    if (strlen(COM_IP) == 0) {
        ESP_LOGE(TAG, "No PC selected. Unable to start unicast communication.");
        return;
    }

    // Cria um socket UDP para enviar mensagens unicast
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket for communication: errno %d", errno);
        return;
    }

    // Configura o endereço do PC de destino
    struct sockaddr_in pc_addr;
    pc_addr.sin_family = AF_INET;
    pc_addr.sin_port = htons(UNICAST_PORT);
    pc_addr.sin_addr.s_addr = inet_addr(COM_IP);

    char message[] = "Hello, PC! I am the ESP32.";

    // Envia a mensagem de saudação
    int err = sendto(sock, message, strlen(message), 0,
                     (struct sockaddr *)&pc_addr, sizeof(pc_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending message to PC: errno %d", errno);
    } else {
        ESP_LOGI(TAG, "Message sent to PC: %s", message);
    }

    // Aguarda um intervalo definido antes de reiniciar as tarefas
    vTaskDelay(pdMS_TO_TICKS(BROADCAST_INTERVAL_MS));

    ESP_LOGI(TAG, "Restarting tasks...");

    // Reinicia a tarefa de aquisição contínua do ADC
    if (xTaskCreate(adc_continuous_task, "adc_continuous_task", 4 * 4096, 
                    NULL, configMAX_PRIORITIES - 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create ADC continuous task");
    }

    // Reinicia a tarefa de transmissão de dados via UDP
    if (xTaskCreate(udp_cast_task, "udp_cast_task", 4096, 
                    NULL, configMAX_PRIORITIES - 15, &udp_cast_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create data transmission task");
    }
    ESP_LOGI(TAG, "Tasks restarted");

    close(sock); // Fecha o socket após o envio
}

/**
 * @brief Tarefa principal de comunicação do ESP32.
 *
 * Esta tarefa envia broadcasts periódicos contendo o IP e o MAC do ESP32,
 * permitindo que um PC possa selecioná-lo para comunicação unicast. Enquanto
 * o dispositivo não for selecionado, a tarefa envia os broadcasts. Ao ser
 * selecionado, a tarefa encerra o broadcast.
 *
 * @param pvParameters Parâmetros da tarefa (não utilizados).
 */
void esp_com_task(void *pvParameters)
{
    // Configura o endereço de broadcast
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(BROADCAST_PORT);

    // Cria um socket UDP para enviar broadcasts
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
    }

    // Habilita a opção de broadcast no socket
    int broadcast_enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        ESP_LOGE(TAG, "Failed to set broadcast option: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
    }

    // Obtém o endereço MAC do ESP32
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Obtém o endereço IP do ESP32
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_get_ip_info(netif, &ip_info);

    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));

    char payload[128]; // Buffer para a mensagem de broadcast

    // Cria a tarefa que escuta a escolha do PC
    xTaskCreate(listen_for_choice_task, "listen_for_choice_task", 4096, NULL, 5, NULL);

    while (1) {
        // Se o dispositivo já foi selecionado, encerra o broadcast
        if (selected) {
            ESP_LOGI(TAG, "Device selected. Stopping broadcast.");
            break;
        }

        // Monta a mensagem de broadcast com IP e MAC do dispositivo
        snprintf(payload, sizeof(payload),
                 "ESP32 Device - IP: %s, MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
                 ip_str, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        int err = sendto(sock, payload, strlen(payload), 0, 
                         (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        } else {
            ESP_LOGI(TAG, "Broadcast sent: %s", payload);
        }

        // Aguarda o intervalo definido antes de enviar o próximo broadcast
        vTaskDelay(pdMS_TO_TICKS(BROADCAST_INTERVAL_MS));
    }

    // Encerra a tarefa após o dispositivo ser selecionado
    close(sock);
    vTaskDelete(NULL);
}

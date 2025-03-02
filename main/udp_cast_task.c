#include <string.h>
#include <arpa/inet.h>
#include <lwipopts.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include "udp_cast_task.h"
#include "adc_continuous_task.h" // Incluir para acesso ao DataPacket
#include "config.h"

#define TAG "UDP_CAST"

// Definindo o handle da tarefa
TaskHandle_t udp_cast_task_handle;

// Variável global para o socket
static int sock = -1;  

// Variável COM_IP, que será atualizada com o IP do PC após o comando SELECTED
char COM_IP[16] = BROADCAST_IP; 

// Variável global para controlar se o dispositivo foi selecionado
int selected = 0;

// Função que transmite o pacote de dados via UDP
void udp_cast_task(void *pvParameters) {

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(COM_IP); // Define o endereço IP de destino
    dest_addr.sin_family = AF_INET;                // Define o protocolo de comunicação como IPv4
    dest_addr.sin_port = htons(DATA_PORT);         // Define a porta de destino em formato de rede

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP); // Cria um socket UDP
    if (sock < 0) {
        ESP_LOGE(TAG, "Erro ao criar o socket: errno %d", errno);
        vTaskDelete(NULL); // Encerra a tarefa caso o socket não seja criado
        return;
    }

    int broadcast_perm = 1; // Ativa o modo de broadcast no socket
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_perm, sizeof(broadcast_perm));

    while (1) {

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Espera uma notificação para iniciar a transmissão

        // Transmitir o pacote de dados
        int err = sendto(sock, &data_packet, sizeof(data_packet), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Erro ao enviar: errno %d", errno);
        } else {
            // Descomente os logs para depuração detalhada durante o envio
            // ESP_LOGI(TAG, "Pacote enviado. Contagem de pacotes: %d", data_packet.packet_count);
            // ESP_LOGI(TAG, "Canais Ativos: %d", data_packet.active_channels);
            // ESP_LOGI(TAG, "Amostras por Canais: %d", data_packet.samples_per_channel);
        }

        // Atraso ajustável para controle da frequência de envio
        // Remova ou ajuste conforme necessário para evitar sobrecarga da rede (APARENTEMENTE não é necessário)
        // vTaskDelay(pdMS_TO_TICKS(1000)); 
    }

    // Fechar o socket e terminar a tarefa
    ESP_LOGW(TAG, "TASK ENCERRADA");
    close(sock); // Fecha o socket antes de encerrar a tarefa
    vTaskDelete(NULL);
}

// Função para reiniciar a tarefa de broadcast UDP
void end_udp_cast_task() {

    // Fecha o socket se ele estiver aberto
    if (sock > -1) {
        close(sock); // Garante que o socket seja fechado corretamente
        sock = -1;  // Reseta o identificador do socket
        ESP_LOGI(TAG, "Socket de cast UDP fechado.");
    }

    // Finaliza a task existente, se ela estiver rodando
    if (udp_cast_task_handle != NULL) {
        vTaskDelete(udp_cast_task_handle);  // Exclui a tarefa atual
        udp_cast_task_handle = NULL;       // Limpa o handle
        ESP_LOGI(TAG, "Tarefa de cast UDP finalizada.");
    }
}

void start_udp_cast_task() {

    // Cria uma nova instância da tarefa
    if (xTaskCreate(udp_cast_task, "udp_cast_task", 4096, NULL, 3, &udp_cast_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Falha ao recriar a tarefa de cast UDP"); // Mensagem de erro caso a tarefa não seja criada
    } else {
        ESP_LOGI(TAG, "Tarefa de cast UDP reiniciada com sucesso."); // Confirmação de que a tarefa foi recriada
    }
}

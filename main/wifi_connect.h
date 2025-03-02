#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "esp_err.h"

/**
 * @brief Inicializa a conexão Wi-Fi no modo STA (Station).
 *
 * @return
 *     - ESP_OK: Conexão bem-sucedida
 *     - ESP_FAIL: Falha ao conectar
 */
esp_err_t wifi_connect();

#endif // WIFI_CONNECT_H
#ifndef CONFIG_H
#define CONFIG_H

//* Este arquivo deve ser ajustado e compilado para que a calibração e conexão do dispositivo seja efetuada corretamente,
//* Verificar as seguintes informações:
// CONFIG_WIFI_SSID - Nome da Rede Wifi (SSID)
// CONFIG_WIFI_PASSWORD - Senha da Rede Wifi
// DC_OffSet - Offset de calibração do nivel DC do ADC
// SPS - Taxa de amostragem
// COEFF_ADC_A - Coeficiente do ADC (ajuste fino)
// COEFF_ADC_B - Coeficiente do ADC (ajuste fino)
// COEFF_CH_1 - Ajuste de calibração para o canal 1
// COEFF_CH_2 - Ajuste de calibração para o canal 2
// COEFF_CH_3 - Ajuste de calibração para o canal 3
// COEFF_CH_4 - Ajuste de calibração para o canal 4
// COEFF_CH_5 - Ajuste de calibração para o canal 5
// COEFF_CH_6 - Ajuste de calibração para o canal 6


//! ------------------- AJUSTES DE ENTRADA -------------------
//! Configurações relacionadas ao número de canais, amostras e filtros
//* Número de canais e amostras
#define MAX_CHANNELS 6           // Número de canais ativos (alterar pode afetar o valor do SPS) (Ref: 6)
#define SAMPLES_PER_CHANNEL 80   // Número de amostras por canal (Ref: 80)
//* Filtros
#define APPLYTHIRANFILTER true          // true para Ativar | false para Desativar
#define APPLYBUTTERWORTHFILTER true     // true para Ativar | false para Desativar
//! -------------------------------------------------------


//! ------------------- AJUSTES DE REDE -------------------
//! Configurações relacionadas à rede (endereços IP, portas e intervalos)
//* Endereço de broadcast (pode ser ajustado para a rede específica)
#define BROADCAST_IP "255.255.255.255"      //Broadcast GERAL
// #define BROADCAST_IP "192.168.255.255"   //Broadcast redes domésticas
// #define BROADCAST_IP "10.10.255.255"     //Broadcast USP
//* Portas de comunicação
#define BROADCAST_PORT 5000      // Porta para broadcast (porta usada para indicar disponibilidade de conexão do ESP)
#define DATA_PORT 5000           // Porta para envio de dados (os dados aquisitados pelo ADC são repassados por essa porta)
#define CHOICE_PORT 6000         // Porta para receber a escolha do PC (porta que escuta o comando "SELECTED" e sincroniza o IP do PC)
#define UNICAST_PORT 7000        // Porta para comunicação unicast (não está sendo usada)
//* Intervalo de tempo para envio de pacotes de broadcast (em milissegundos)
#define BROADCAST_INTERVAL_MS 1000      // (Ref: 1000)
//* Configurações de Wi-Fi
#define CONFIG_WIFI_SSID "SSID"                             // SSID da rede Wi-Fi
#define CONFIG_WIFI_PASSWORD "Password"                     // Senha da rede Wi-Fi
#define CONFIG_WIFI_MAXIMUM_RETRY 5                         // Número máximo de tentativas de conexão (Ref: 5)
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN    // Nível mínimo de autenticação (Ref: WIFI_AUTH_OPEN)
//! -------------------------------------------------------


//! ----------------- CONFIGURAÇÕES DO ADC ----------------
//* Ajustes específicos para o ESP32 e ESP32S2
#define DC_OffSet 1860                  // Offset de calibração do nivel DC do ADC (Ref: 1860)
#define SPS 5867                        // Taxa de amostragem (Samples Per Second) (Ref: 5860)
#define COEFF_ATTEN ADC_ATTEN_DB_12     // Atenuação do ADC (Ref: ADC_ATTEN_DB_12)
#define COEFF_ADC_A 0                   // Coeficiente do ADC (ajuste fino)
#define COEFF_ADC_B 0                   // Coeficiente do ADC (ajuste fino)
#define COEFF_CH_1 0                    // Ajuste de calibração para o canal 1
#define COEFF_CH_2 0                    // Ajuste de calibração para o canal 2
#define COEFF_CH_3 0                    // Ajuste de calibração para o canal 3
#define COEFF_CH_4 0                    // Ajuste de calibração para o canal 4
#define COEFF_CH_5 0                    // Ajuste de calibração para o canal 5
#define COEFF_CH_6 0                    // Ajuste de calibração para o canal 6
//! -------------------------------------------------------


//! ---------------- CONFIGURAÇÕES AVANÇADAS ----------------
//! Não modificar estas definições, pois são necessárias para compatibilidade
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define ADC_CHANNEL p_data->type1.channel
#define ADC_DATA p_data->type1.data
#else
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define ADC_CHANNEL p_data->type2.channel
#define ADC_DATA p_data->type2.data
#endif
//! --------------------------------------------------------- 

#endif // CONFIG_H

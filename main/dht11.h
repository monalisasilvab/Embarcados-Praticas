#ifndef DHT11_H
#define DHT11_H

#include "esp_err.h"
#include <stdint.h>

// Estrutura para armazenar dados do DHT11
typedef struct {
    float temperature;
    float humidity;
    bool is_valid;
} dht11_data_t;

/**
 * @brief Inicializa o DHT11
 * @param gpio_pin Pino GPIO para o DHT11 (D2 = GPIO2)
 * @return ESP_OK se sucesso, ESP_FAIL se erro
 */
esp_err_t dht11_init(int gpio_pin);

/**
 * @brief Lê dados do DHT11
 * @param data Ponteiro para estrutura onde os dados serão armazenados
 * @return ESP_OK se sucesso, ESP_FAIL se erro
 */
esp_err_t dht11_read(dht11_data_t *data);

/**
 * @brief Lê dados do DHT11 com múltiplas tentativas
 * @param data Ponteiro para estrutura onde os dados serão armazenados
 * @param max_attempts Número máximo de tentativas
 * @return ESP_OK se sucesso, ESP_FAIL se erro
 */
esp_err_t dht11_read_with_retry(dht11_data_t *data, int max_attempts);

/**
 * @brief Testa a conexão com o DHT11
 * @return ESP_OK se sensor responde, ESP_FAIL se não responde
 */
esp_err_t dht11_test_connection(void);

/**
 * @brief Converte dados do DHT11 para string JSON
 * @param data Dados do DHT11
 * @param json_str Buffer para armazenar JSON
 * @param json_len Tamanho do buffer
 * @return ESP_OK se sucesso, ESP_FAIL se erro
 */
esp_err_t dht11_to_json(dht11_data_t *data, char *json_str, size_t json_len);

#endif // DHT11_H

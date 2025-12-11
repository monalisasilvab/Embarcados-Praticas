#ifndef STATUS_LED_H
#define STATUS_LED_H

#include "esp_err.h"
#include <stdbool.h>

// Configuração do LED de status
#define STATUS_LED_GPIO 21  // GPIO 21 (D21)

/**
 * @brief Inicializa o LED de status
 * @return ESP_OK se sucesso, ESP_FAIL se erro
 */
esp_err_t status_led_init(void);

/**
 * @brief Liga o LED de status
 */
void status_led_on(void);

/**
 * @brief Desliga o LED de status
 */
void status_led_off(void);

/**
 * @brief Alterna o estado do LED de status
 */
void status_led_toggle(void);

/**
 * @brief Define o modo de erro (piscando) ou normal (aceso)
 * @param error_mode true para modo erro (piscando), false para normal (aceso)
 */
void status_led_set_error(bool error_mode);

/**
 * @brief Verifica se está em modo de erro
 * @return true se em modo erro, false se normal
 */
bool status_led_is_error_mode(void);

/**
 * @brief Task do LED de status (deve ser chamada em uma task separada)
 * @param pvParameters Parâmetros da task (não usado)
 */
void status_led_task(void *pvParameters);

#endif // STATUS_LED_H

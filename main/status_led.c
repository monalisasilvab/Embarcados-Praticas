#include "status_led.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>

static const char *TAG = "STATUS_LED";

// Variáveis de controle
static bool led_initialized = false;
static bool led_state = false;          // Estado atual do LED (on/off)
static bool error_mode = false;         // Modo erro (piscando) ou normal
static bool led_enabled = true;         // LED habilitado

esp_err_t status_led_init(void) {
    // Configuração do GPIO como saída
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << STATUS_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&led_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao configurar GPIO %d para LED", STATUS_LED_GPIO);
        return ESP_FAIL;
    }
    
    // Estado inicial: LED desligado
    gpio_set_level(STATUS_LED_GPIO, 0);
    led_state = false;
    error_mode = false;
    led_initialized = true;
    
    ESP_LOGI(TAG, "LED de status inicializado no GPIO %d", STATUS_LED_GPIO);
    return ESP_OK;
}

void status_led_on(void) {
    if (!led_initialized) {
        ESP_LOGW(TAG, "LED não inicializado");
        return;
    }
    
    if (led_enabled) {
        gpio_set_level(STATUS_LED_GPIO, 1);
        led_state = true;
    }
}

void status_led_off(void) {
    if (!led_initialized) {
        ESP_LOGW(TAG, "LED não inicializado");
        return;
    }
    
    gpio_set_level(STATUS_LED_GPIO, 0);
    led_state = false;
}

void status_led_toggle(void) {
    if (!led_initialized) {
        ESP_LOGW(TAG, "LED não inicializado");
        return;
    }
    
    if (led_state) {
        status_led_off();
    } else {
        status_led_on();
    }
}

void status_led_set_error(bool error_mode_new) {
    if (!led_initialized) {
        ESP_LOGW(TAG, "LED não inicializado");
        return;
    }
    
    error_mode = error_mode_new;
    
    if (error_mode) {
        ESP_LOGW(TAG, "LED em modo ERRO - piscando");
    } else {
        ESP_LOGI(TAG, "LED em modo NORMAL - aceso contínuo");
        status_led_on();  // Liga LED no modo normal
    }
}

bool status_led_is_error_mode(void) {
    return error_mode;
}

void status_led_task(void *pvParameters) {
    ESP_LOGI(TAG, "Iniciando task do LED de status");
    
    while (1) {
        if (!led_initialized) {
            ESP_LOGE(TAG, "LED não inicializado, finalizando task");
            vTaskDelete(NULL);
            return;
        }
        
        if (error_mode) {
            // Modo erro: pisca rápido (500ms ligado, 500ms desligado)
            status_led_toggle();
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            // Modo normal: LED sempre aceso
            if (!led_state) {
                status_led_on();
            }
            vTaskDelay(pdMS_TO_TICKS(1000)); // Verifica a cada 1 segundo
        }
    }
}

// Função auxiliar para indicar diferentes tipos de erro com padrões de piscada
void status_led_error_pattern(int pattern) {
    if (!led_initialized || !error_mode) {
        return;
    }
    
    switch (pattern) {
        case 1: // Erro WiFi: pisca 1 vez e pausa
            status_led_on();
            vTaskDelay(pdMS_TO_TICKS(200));
            status_led_off();
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
            
        case 2: // Erro MQTT: pisca 2 vezes e pausa
            for (int i = 0; i < 2; i++) {
                status_led_on();
                vTaskDelay(pdMS_TO_TICKS(200));
                status_led_off();
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            vTaskDelay(pdMS_TO_TICKS(800));
            break;
            
        case 3: // Erro DHT11: pisca 3 vezes e pausa
            for (int i = 0; i < 3; i++) {
                status_led_on();
                vTaskDelay(pdMS_TO_TICKS(200));
                status_led_off();
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            vTaskDelay(pdMS_TO_TICKS(600));
            break;
            
        default: // Erro geral: pisca contínuo
            status_led_toggle();
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
    }
}

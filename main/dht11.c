#include "dht11.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "DHT11";
static int dht11_gpio = -1;

esp_err_t dht11_init(int gpio_pin) {
    dht11_gpio = gpio_pin;
    
    ESP_LOGI(TAG, "=== INICIALIZANDO DHT11 ===");
    ESP_LOGI(TAG, "GPIO selecionado: %d", gpio_pin);
    
    // Reset do GPIO primeiro
    gpio_reset_pin(gpio_pin);
    
    // Configuração inicial como saída
    gpio_set_direction(gpio_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio_pin, 1);
    
    // Aguarda estabilização
    vTaskDelay(pdMS_TO_TICKS(2000));  // 2 segundos como no Arduino
    
    ESP_LOGI(TAG, "✅ DHT11 inicializado no GPIO %d", gpio_pin);
    ESP_LOGI(TAG, "Certifique-se de ter resistor pull-up de 10kΩ entre DATA e VCC");
    return ESP_OK;
}

esp_err_t dht11_read(dht11_data_t *data) {
    if (dht11_gpio == -1) {
        ESP_LOGE(TAG, "DHT11 não inicializado");
        return ESP_FAIL;
    }
    
    if (data == NULL) {
        ESP_LOGE(TAG, "Ponteiro de dados inválido");
        return ESP_FAIL;
    }
    
    uint8_t raw_data[5] = {0, 0, 0, 0, 0};
    uint8_t checksum;
    uint64_t timeout_start;
    
    // Inicializa dados como inválidos
    data->is_valid = false;
    data->temperature = 0.0;
    data->humidity = 0.0;
    
    ESP_LOGI(TAG, "=== INICIANDO LEITURA DHT11 - GPIO %d ===", dht11_gpio);
    
    // Inicia comunicação (como no Arduino)
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_gpio, 0);        // LOW
    vTaskDelay(pdMS_TO_TICKS(18));        // 18ms
    gpio_set_level(dht11_gpio, 1);        // HIGH
    ets_delay_us(40);                     // 40us
    gpio_set_direction(dht11_gpio, GPIO_MODE_INPUT);  // INPUT_PULLUP equivalente
    
    // Aguarda resposta do sensor
    timeout_start = esp_timer_get_time();
    while (gpio_get_level(dht11_gpio) == 1) {
        if ((esp_timer_get_time() - timeout_start) > 100) {
            ESP_LOGW(TAG, "Timeout - Sensor não respondeu");
            return ESP_FAIL;
        }
    }
    
    // Confirma presença do sensor - LOW
    timeout_start = esp_timer_get_time();
    while (gpio_get_level(dht11_gpio) == 0) {
        if ((esp_timer_get_time() - timeout_start) > 100) {
            ESP_LOGW(TAG, "Timeout - Sinal LOW muito longo");
            return ESP_FAIL;
        }
    }
    
    // Confirma presença do sensor - HIGH
    timeout_start = esp_timer_get_time();
    while (gpio_get_level(dht11_gpio) == 1) {
        if ((esp_timer_get_time() - timeout_start) > 100) {
            ESP_LOGW(TAG, "Timeout - Sinal HIGH muito longo");
            return ESP_FAIL;
        }
    }
    
    ESP_LOGI(TAG, "Sensor respondeu, lendo 40 bits...");
    
    // Lê os 40 bits de dados (exatamente como no Arduino)
    for (int i = 0; i < 40; i++) {
        // Aguarda início do bit (LOW -> HIGH)
        timeout_start = esp_timer_get_time();
        while (gpio_get_level(dht11_gpio) == 0) {
            if ((esp_timer_get_time() - timeout_start) > 100) {
                ESP_LOGW(TAG, "Timeout - Bit %d LOW muito longo", i);
                return ESP_FAIL;
            }
        }
        
        // Mede duração do pulso HIGH
        uint64_t duration_start = esp_timer_get_time();
        timeout_start = esp_timer_get_time();
        while (gpio_get_level(dht11_gpio) == 1) {
            if ((esp_timer_get_time() - timeout_start) > 100) {
                ESP_LOGW(TAG, "Timeout - Bit %d HIGH muito longo", i);
                return ESP_FAIL;
            }
        }
        
        uint64_t duration = esp_timer_get_time() - duration_start;
        
        // Processa bit (como no Arduino)
        raw_data[i/8] <<= 1;
        if (duration > 40) {
            raw_data[i/8] |= 1;
        }
        
        // Log a cada byte para debug
        if ((i + 1) % 8 == 0) {
            ESP_LOGI(TAG, "Byte %d: 0x%02X", i/8, raw_data[i/8]);
        }
    }
    
    // Verifica checksum (como no Arduino)
    checksum = raw_data[0] + raw_data[1] + raw_data[2] + raw_data[3];
    
    ESP_LOGI(TAG, "Dados brutos: %02X %02X %02X %02X %02X", 
             raw_data[0], raw_data[1], raw_data[2], raw_data[3], raw_data[4]);
    ESP_LOGI(TAG, "Checksum calculado: %02X, recebido: %02X", checksum, raw_data[4]);
    
    if (raw_data[4] != checksum) {
        ESP_LOGW(TAG, "Erro de checksum - Dados corrompidos");
        return ESP_FAIL;
    }
    
    // Converte dados (como no Arduino)
    data->humidity = (float)raw_data[0] + (float)raw_data[1] / 10.0;
    data->temperature = (float)raw_data[2] + (float)raw_data[3] / 10.0;
    data->is_valid = true;
    
    ESP_LOGI(TAG, "=== DHT11 FUNCIONANDO ===");
    ESP_LOGI(TAG, "🌡️  Temperatura: %.1f°C", data->temperature);
    ESP_LOGI(TAG, "💧 Umidade:     %.1f%%", data->humidity);
    ESP_LOGI(TAG, "✅ Checksum:    %02X (OK)", raw_data[4]);
    ESP_LOGI(TAG, "========================");
    
    return ESP_OK;
}

esp_err_t dht11_to_json(dht11_data_t *data, char *json_str, size_t json_len) {
    if (data == NULL || json_str == NULL) {
        return ESP_FAIL;
    }
    
    int64_t timestamp = esp_timer_get_time() / 1000;
    
    int ret = snprintf(json_str, json_len,
        "{"
        "\"device_id\":\"ESP32_Client\","
        "\"sensor\":\"DHT11\","
        "\"temperature\":%.1f,"
        "\"humidity\":%.1f,"
        "\"valid\":%s,"
        "\"timestamp\":%lld"
        "}",
        data->temperature,
        data->humidity,
        data->is_valid ? "true" : "false",
        timestamp
    );
    
    if (ret < 0 || ret >= json_len) {
        ESP_LOGE(TAG, "Erro ao gerar JSON");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// Função simplificada para testar conexão
esp_err_t dht11_test_connection(void) {
    if (dht11_gpio == -1) {
        ESP_LOGE(TAG, "DHT11 não inicializado");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "=== TESTE RÁPIDO DHT11 ===");
    ESP_LOGI(TAG, "GPIO: %d", dht11_gpio);
    
    dht11_data_t test_data;
    esp_err_t result = dht11_read(&test_data);
    
    if (result == ESP_OK && test_data.is_valid) {
        ESP_LOGI(TAG, "✅ Sensor funcionando: %.1f°C, %.1f%%", 
                 test_data.temperature, test_data.humidity);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "❌ Sensor não respondeu adequadamente");
        ESP_LOGW(TAG, "Verifique:");
        ESP_LOGW(TAG, "- Alimentação 3.3V/5V estável");
        ESP_LOGW(TAG, "- GND conectado");
        ESP_LOGW(TAG, "- Resistor pull-up 10kΩ entre DATA e VCC");
        ESP_LOGW(TAG, "- Conexões firmes");
        return ESP_FAIL;
    }
}

// Função com múltiplas tentativas (simplificada)
esp_err_t dht11_read_with_retry(dht11_data_t *data, int max_attempts) {
    esp_err_t result = ESP_FAIL;
    
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        ESP_LOGI(TAG, "🔄 Tentativa %d/%d", attempt, max_attempts);
        
        result = dht11_read(data);
        
        if (result == ESP_OK && data->is_valid) {
            ESP_LOGI(TAG, "✅ Sensor funcionando corretamente!");
            return ESP_OK;
        }
        
        if (attempt < max_attempts) {
            ESP_LOGW(TAG, "❌ Tentativa %d falhou, aguardando 3 segundos...", attempt);
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }
    
    ESP_LOGE(TAG, "❌ Todas as %d tentativas falharam", max_attempts);
    ESP_LOGE(TAG, "Verifique hardware:");
    ESP_LOGE(TAG, "- Alimentação correta");
    ESP_LOGE(TAG, "- Resistor pull-up 10kΩ");
    ESP_LOGE(TAG, "- Conexões soldadas");
    ESP_LOGE(TAG, "- DHT11 não danificado");
    return ESP_FAIL;
}

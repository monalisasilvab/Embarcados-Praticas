#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "nvs_flash.h"
#include "esp_log.h"

#include "wifi_manager.h"
#include "mqtt_manager.h"

#include "rom/ets_sys.h"


// #define WIFI_SSID "UFC_QUIXADA"
// #define WIFI_SSID "ExpoSE"
// #define WIFI_PASS ""

#define WIFI_SSID "brisa-2504280"
#define WIFI_PASS "eubcidpn"

#define AWS_IOT_ENDPOINT "a1gqpq2oiyi1r1-ats.iot.us-east-1.amazonaws.com"
#define AWS_IOT_CLIENT_ID "ESP32_Client"

//Sensores
#define UV_SENSOR_GPIO 34
#define SOIL_MOISTURE_GPIO 35
#define DHT11_GPIO 4

//Atuadores
#define SOLENOID_GPIO 18

//Topicos
#define TOPIC_UV_SENSOR "esp32/uv"
#define TOPIC_SOIL_MOISTURE "esp32/soil_moisture"
#define TOPIC_DHT11 "esp32/dht11"
#define TOPIC_SOLENOID "esp32/solenoid"

extern const uint8_t aws_root_ca_pem_start[] asm("");
extern const uint8_t device_certificate_pem_crt_start[] asm("");
extern const uint8_t device_private_pem_key_start[] asm("");

esp_mqtt_client_handle_t client = NULL;
extern bool mqtt_connected;

// Protótipos das tasks
void publish_sensor_data_task(void *pvParameters);
void button_status_task(void *pvParameters);

esp_err_t dht11_read(int gpio, int16_t *humidity, int16_t *temperature) {
    uint8_t data[5] = {0};
    uint8_t byte = 0, bit = 7;
    int i = 0;
    *humidity = 0;
    *temperature = 0;

    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio, 0);
    ets_delay_us(18000); // 18ms
    gpio_set_level(gpio, 1);
    ets_delay_us(30); // 20-40us
    gpio_set_direction(gpio, GPIO_MODE_INPUT);

    uint32_t timeout = 0;
    while (gpio_get_level(gpio) == 1) { if (++timeout > 100) return ESP_FAIL; ets_delay_us(1); }
    timeout = 0;
    while (gpio_get_level(gpio) == 0) { if (++timeout > 100) return ESP_FAIL; ets_delay_us(1); }
    timeout = 0;
    while (gpio_get_level(gpio) == 1) { if (++timeout > 100) return ESP_FAIL; ets_delay_us(1); }

    // Lê 40 bits
    for (i = 0; i < 40; i++) {
        // Espera início do bit
        timeout = 0;
        while (gpio_get_level(gpio) == 0) { if (++timeout > 70) return ESP_FAIL; ets_delay_us(1); }
        // Mede duração do pulso alto
        int t = 0;
        while (gpio_get_level(gpio) == 1) { t++; if (t > 100) return ESP_FAIL; ets_delay_us(1); }
        // Se pulso > 40us, é 1, senão 0
        if (t > 40) data[byte] |= (1 << bit);
        if (bit == 0) { bit = 7; byte++; } else { bit--; }
    }
    // Checa checksum
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) return ESP_FAIL;
    *humidity = data[0];
    *temperature = data[2];
    return ESP_OK;
}

void app_main(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_manager_init(WIFI_SSID, WIFI_PASS, WIFI_AUTH_OPEN);

    vTaskDelay(pdMS_TO_TICKS(5000));

    mqtt_manager_start(AWS_IOT_ENDPOINT, AWS_IOT_CLIENT_ID,aws_root_ca_pem_start, device_certificate_pem_crt_start, device_private_pem_key_start, &client);
    vTaskDelay(pdMS_TO_TICKS(3000));

    xTaskCreate(button_status_task, "button_task", 2048, NULL, 5, NULL);

    ESP_LOGI("APP_MAIN", "Aplicação iniciada com sucesso!");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}

// Implementação das tasks
void publish_sensor_data_task(void *pvParameters) {
    int counter = 0;
    char message[200];
    int16_t temperature = 0, humidity = 0;
    while (1) {
        if (mqtt_connected) {
            esp_err_t res = dht11_read(DHT11_GPIO, &humidity, &temperature);
            if (res == ESP_OK) {
                int64_t timestamp_ms = esp_timer_get_time() / 1000;
                snprintf(message, sizeof(message),
                    "{\"device_id\":\"%s\",\"Temperatura\":%d,\"humidade\":%d,\"cnt\":%d,\"tempo\":%lld}",
                    AWS_IOT_CLIENT_ID, temperature, humidity, counter, timestamp_ms);
                esp_mqtt_client_publish(client, "esp32/data", message, 0, 1, 0);
                ESP_LOGI("PUBLISH_TASK", "Publicado: %s", message);
            } else {
                ESP_LOGW("PUBLISH_TASK", "Falha ao ler DHT11: %d", res);
            }
            counter++;
        } else {
            ESP_LOGW("PUBLISH_TASK", "MQTT não conectado, aguardando...");
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}



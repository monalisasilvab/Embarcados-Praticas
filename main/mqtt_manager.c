#include "mqtt_manager.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "MQTT_MANAGER";
bool mqtt_connected = false;

#define AWS_IOT_LED_TOPIC "esp32/ledrgb"
#define LED_R_GPIO 5
#define LED_G_GPIO 18
#define LED_B_GPIO 19

static void set_led_rgb(const char *cmd, int len) {
    int r = 0, g = 0, b = 0;
    if (len == 1 && cmd[0] == '0') {
        r = g = b = 0;
    } else {
        for (int i = 0; i < len; ++i) {
            if (cmd[i] == 'r' || cmd[i] == 'R') r = 1;
            if (cmd[i] == 'g' || cmd[i] == 'G') g = 1;
            if (cmd[i] == 'b' || cmd[i] == 'B') b = 1;
        }
    }
    gpio_set_level(LED_R_GPIO, r);
    gpio_set_level(LED_G_GPIO, g);
    gpio_set_level(LED_B_GPIO, b);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt_connected = true;
        // Subscreve ao tópico do LED RGB
        msg_id = esp_mqtt_client_subscribe(client, AWS_IOT_LED_TOPIC, 1);
        ESP_LOGI(TAG, "Subscrito ao tópico %s, msg_id=%d", AWS_IOT_LED_TOPIC, msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        if (strncmp(event->topic, AWS_IOT_LED_TOPIC, event->topic_len) == 0) {
            set_led_rgb(event->data, event->data_len);
            ESP_LOGI(TAG, "Comando LED RGB recebido: %.*s", event->data_len, event->data);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected = false;
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        break;
    }
}

void mqtt_manager_start(const char *endpoint, const char *client_id,
                       const uint8_t *root_ca, const uint8_t *device_cert, const uint8_t *device_key,
                       esp_mqtt_client_handle_t *out_client)
{
    char mqtt_url[256];
    snprintf(mqtt_url, sizeof(mqtt_url), "mqtts://%s:8883", endpoint);
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_url,
        .broker.verification.certificate = (const char *)root_ca,
        .credentials = {
            .authentication = {
                .certificate = (const char *)device_cert,
                .key = (const char *)device_key,
            },
            .client_id = client_id,
        }
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Falha ao inicializar cliente MQTT");
        *out_client = NULL;
        return;
    }
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
    *out_client = client;
}

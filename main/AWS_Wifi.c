#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "driver/gpio.h"

static const char *TAG = "AWS_IOT";

#define WIFI_SSID "UFC_QUIXADA"
#define WIFI_PASS ""

#define WIFI_SSID_1 "brisa-2504280"
#define WIFI_PASS_1 "eubcidpn"

#define AWS_IOT_PORT 8883  // Porta segura TLS

#define AWS_IOT_ENDPOINT "a1gqpq2oiyi1r1-ats.iot.sa-east-1.amazonaws.com"
#define AWS_IOT_TOPIC "esp32/data"
#define AWS_IOT_CLIENT_ID "ESP32_Client"

#define LED_R_GPIO 5   // D5
#define LED_G_GPIO 18  // D18
#define LED_B_GPIO 19  // D19
#define AWS_IOT_LED_TOPIC "esp32/ledrgb"

#define BUTTON_GPIO 21  // D21
#define AWS_IOT_BUTTON_TOPIC "esp32/button"

extern const uint8_t amazon_root_ca1_pem_start[] asm("_binary_AmazonRootCA1_pem_start");
extern const uint8_t amazon_root_ca1_pem_end[] asm("_binary_AmazonRootCA1_pem_end");
extern const uint8_t device_certificate_pem_crt_start[] asm("_binary_21799d82cb8e53178d50791a8bce841ab0132bdc3596a4350b798c0b5dc0925a_certificate_pem_crt_start");
extern const uint8_t device_certificate_pem_crt_end[] asm("_binary_21799d82cb8e53178d50791a8bce841ab0132bdc3596a4350b798c0b5dc0925a_certificate_pem_crt_end");
extern const uint8_t device_private_pem_key_start[] asm("_binary_21799d82cb8e53178d50791a8bce841ab0132bdc3596a4350b798c0b5dc0925a_private_pem_key_start");
extern const uint8_t device_private_pem_key_end[] asm("_binary_21799d82cb8e53178d50791a8bce841ab0132bdc3596a4350b798c0b5dc0925a_private_pem_key_end");

// Aliases para facilitar a leitura
#define aws_root_ca_start amazon_root_ca1_pem_start
#define device_cert_start device_certificate_pem_crt_start  
#define device_key_start device_private_pem_key_start

static esp_mqtt_client_handle_t client = NULL;
static bool mqtt_connected = false;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void set_led_rgb(const char *cmd, int len) {
    int r = 0, g = 0, b = 0;
    if (len == 1 && cmd[0] == '0') {
        // Desliga tudo
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
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt_connected = true;
        // Subscrever ao tópico para receber mensagens
        msg_id = esp_mqtt_client_subscribe(client, AWS_IOT_TOPIC, 1);
        ESP_LOGI(TAG, "Subscrito ao tópico %s, msg_id=%d", AWS_IOT_TOPIC, msg_id);
        msg_id = esp_mqtt_client_subscribe(client, AWS_IOT_LED_TOPIC, 1);
        ESP_LOGI(TAG, "Subscrito ao tópico %s, msg_id=%d", AWS_IOT_LED_TOPIC, msg_id);
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected = false;
        break;
        
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("=== MENSAGEM RECEBIDA ===\n");
        printf("TÓPICO: %.*s\n", event->topic_len, event->topic);
        printf("DADOS: %.*s\n", event->data_len, event->data);
        printf("========================\n");
        // Controle do LED RGB
        if (strncmp(event->topic, AWS_IOT_LED_TOPIC, event->topic_len) == 0) {
            set_led_rgb(event->data, event->data_len);
            ESP_LOGI(TAG, "Comando LED RGB recebido: %.*s", event->data_len, event->data);
        }
        break;
        
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
        
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi desconectado, tentando reconectar...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WiFi conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode =  WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi iniciado. Conectando ao SSID:%s", WIFI_SSID);
}

static void mqtt_app_start(void)
{
    char mqtt_url[256];
    snprintf(mqtt_url, sizeof(mqtt_url), "mqtts://%s:8883", AWS_IOT_ENDPOINT);
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_url,
        .broker.verification.certificate = (const char *)aws_root_ca_start,
        .credentials = {
            .authentication = {
                .certificate = (const char *)device_cert_start,
                .key = (const char *)device_key_start,
            },
            .client_id = AWS_IOT_CLIENT_ID,
        }
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Falha ao inicializar cliente MQTT");
        return;
    }
    
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
}

void publish_sensor_data_task(void *pvParameters)
{
    int counter = 0;
    char message[200];
    
    while (1) {
        if (mqtt_connected) {
            // Simula dados de sensores
            float temperature = 25.0 + (float)(counter % 10);
            float humidity = 60.0 + (float)(counter % 20);
            
            // Obter timestamp em microssegundos e converter para milissegundos
            int64_t timestamp_ms = esp_timer_get_time() / 1000;
            
            snprintf(message, sizeof(message), 
                    "{\"device_id\":\"%s\",\"Temperatura\":%.2f,\"humidade\":%.2f,\"cnt\":%d,\"tempo\":%lld}",
                    AWS_IOT_CLIENT_ID, temperature, humidity, counter, timestamp_ms);
            
            ESP_LOGI(TAG, "Publicando: %s", message);
            
            int msg_id = esp_mqtt_client_publish(client, AWS_IOT_TOPIC, message, 0, 1, 0);
            if (msg_id > 0) {
                ESP_LOGI(TAG, "Mensagem publicada com sucesso, msg_id=%d", msg_id);
            } else {
                ESP_LOGE(TAG, "Falha ao publicar mensagem");
            }
            
            counter++;
        } else {
            ESP_LOGW(TAG, "MQTT não conectado, aguardando...");
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Publica a cada 10 segundos
    }
}

void button_status_task(void *pvParameters)
{
    int last_state = 1;
    while (1) {
        int state = gpio_get_level(BUTTON_GPIO);
        if (state != last_state) {
            last_state = state;
            if (mqtt_connected) {
                const char *status = (state == 0) ? "Solto" : "Apertado";
                esp_mqtt_client_publish(client, AWS_IOT_BUTTON_TOPIC, status, 0, 1, 0);
                ESP_LOGI(TAG, "Botão D21: %s", status);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Checa a cada 100ms
    }
}

// Este arquivo foi migrado para main.c, wifi_manager.c e mqtt_manager.c
// Funções e lógica agora estão separadas por responsabilidade.

void app_main(void)
{
    gpio_reset_pin(LED_R_GPIO);
    gpio_set_direction(LED_R_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_R_GPIO, 0);
    
    gpio_reset_pin(LED_G_GPIO);
    gpio_set_direction(LED_G_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_G_GPIO, 0);
    
    gpio_reset_pin(LED_B_GPIO);
    gpio_set_direction(LED_B_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_B_GPIO, 0);
    
    gpio_reset_pin(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
    
    esp_err_t ret = nvs_flash_init();
    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    wifi_init_sta();
    vTaskDelay(pdMS_TO_TICKS(5000));
    mqtt_app_start();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    xTaskCreate(publish_sensor_data_task, "publish_task", 4096, NULL, 5, NULL);
    xTaskCreate(button_status_task, "button_task", 2048, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Aplicação iniciada com sucesso!");
}

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include <time.h>

// ========== CONFIGURE AQUI ==========
#define WIFI_SSID "UFC_QUIXADA"           // ← SEU WIFI
#define WIFI_PASS ""                       // ← SUA SENHA (vazio se for rede aberta)
#define AWS_IOT_ENDPOINT "a1gqpq2oiyi1r1-ats.iot.us-east-1.amazonaws.com"  // ← SEU ENDPOINT AWS
#define CLIENT_ID "esp32_teste"            // ← MESMO NOME DA THING
#define TOPIC "esp32/test"                 // ← TÓPICO PARA PUBLICAR
// =====================================

static const char *TAG = "SIMPLE_AWS";

// Certificados embedados
extern const uint8_t root_ca_start[] asm("_binary_AmazonRootCA1_pem_start");
extern const uint8_t cert_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t key_start[] asm("_binary_private_pem_key_start");

// Variáveis globais
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool wifi_connected = false;
static bool mqtt_connected = false;
static bool sntp_done = false;

// ========== WiFi Event Handler ==========
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi desconectado, reconectando...");
        wifi_connected = false;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "✅ WiFi conectado!");
        wifi_connected = true;
    }
}

// ========== WiFi Init ==========
void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi iniciado, conectando em %s...", WIFI_SSID);
}

// ========== NTP Sync ==========
void sync_time(void)
{
    if (sntp_done) return;
    
    ESP_LOGI(TAG, "Sincronizando NTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Aguardar sincronização
    int retry = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 10) {
        ESP_LOGI(TAG, "Aguardando NTP... (%d/10)", retry);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    if (timeinfo.tm_year > (2024 - 1900)) {
        char buf[64];
        strftime(buf, sizeof(buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "✅ Tempo: %s", buf);
        sntp_done = true;
    } else {
        ESP_LOGW(TAG, "⚠️ NTP falhou, continuando mesmo assim...");
    }
}

// ========== MQTT Event Handler ==========
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "🎉 MQTT CONECTADO!");
            mqtt_connected = true;
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "⚠️ MQTT desconectado");
            mqtt_connected = false;
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "❌ MQTT ERRO");
            if (event->error_handle && event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Erro TLS: 0x%x", event->error_handle->esp_tls_last_esp_err);
            }
            break;
            
        default:
            break;
    }
}

// ========== MQTT Init ==========
void mqtt_init(void)
{
    char uri[256];
    snprintf(uri, sizeof(uri), "mqtts://%s:8883", AWS_IOT_ENDPOINT);
    
    ESP_LOGI(TAG, "Iniciando MQTT...");
    ESP_LOGI(TAG, "Endpoint: %s", AWS_IOT_ENDPOINT);
    ESP_LOGI(TAG, "Client ID: %s", CLIENT_ID);
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .broker.verification.certificate = (const char *)root_ca_start,
        .credentials = {
            .authentication = {
                .certificate = (const char *)cert_start,
                .key = (const char *)key_start,
            },
            .client_id = CLIENT_ID,
        },
        .network.timeout_ms = 10000,
        .buffer.size = 2048,
        .buffer.out_size = 2048,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

// ========== Publish Task ==========
void publish_task(void *pvParameters)
{
    int counter = 0;
    const char *messages[] = {"teste", "hello world"};
    
    while (1) {
        if (mqtt_connected) {
            const char *msg = messages[counter % 2];
            char payload[100];
            snprintf(payload, sizeof(payload), "{\"message\":\"%s\",\"count\":%d}", msg, counter);
            
            printf("\n📤 Publicando #%d: %s\n", counter + 1, payload);
            
            int msg_id = esp_mqtt_client_publish(mqtt_client, TOPIC, payload, 0, 1, 0);
            if (msg_id >= 0) {
                ESP_LOGI(TAG, "✅ Publicado (msg_id: %d)", msg_id);
            } else {
                ESP_LOGE(TAG, "❌ Falha ao publicar");
            }
            
            counter++;
        } else {
            ESP_LOGW(TAG, "⏳ Aguardando MQTT conectar...");
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // Publicar a cada 2 segundos
    }
}

// ========== MAIN ==========
void app_main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║    ESP32 → AWS IoT Core (SIMPLES)     ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    // 1. Inicializar NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "✅ NVS OK");
    
    // 2. Conectar WiFi
    wifi_init();
    
    // Aguardar WiFi
    int retry = 0;
    while (!wifi_connected && retry++ < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    if (!wifi_connected) {
        ESP_LOGE(TAG, "❌ WiFi não conectou!");
        return;
    }
    
    // 3. Sincronizar NTP
    vTaskDelay(pdMS_TO_TICKS(2000));
    sync_time();
    
    // 4. Conectar MQTT
    vTaskDelay(pdMS_TO_TICKS(1000));
    mqtt_init();
    
    // 5. Aguardar MQTT conectar
    retry = 0;
    while (!mqtt_connected && retry++ < 15) {
        ESP_LOGI(TAG, "Aguardando MQTT... (%d/15)", retry);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    if (!mqtt_connected) {
        ESP_LOGE(TAG, "❌ MQTT não conectou!");
        ESP_LOGE(TAG, "Verifique:");
        ESP_LOGE(TAG, "1. Certificados estão em main/certs/");
        ESP_LOGE(TAG, "2. Thing existe no AWS IoT");
        ESP_LOGE(TAG, "3. Certificado está ativo e anexado");
        ESP_LOGE(TAG, "4. Policy permite tudo");
        return;
    }
    
    // 6. Iniciar publicações
    printf("\n✅ SISTEMA PRONTO!\n");
    printf("📤 Publicando em: %s\n", TOPIC);
    printf("⏱️  Intervalo: 2 segundos\n\n");
    
    xTaskCreate(publish_task, "publish", 4096, NULL, 5, NULL);
    
    // Loop principal
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Status: WiFi=%s MQTT=%s", 
                 wifi_connected ? "OK" : "OFF",
                 mqtt_connected ? "OK" : "OFF");
    }
}

#include "mqtt_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MQTT_MANAGER";
bool mqtt_connected = false;

// Handler customizado
static mqtt_custom_event_handler_t custom_handler = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "✅ MQTT_EVENT_CONNECTED - Conectado ao AWS IoT!");
        mqtt_connected = true;
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "📩 MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "Tópico: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "Dados: %.*s", event->data_len, event->data);

        if (custom_handler != NULL) {
            custom_handler(handler_args, base, event_id, event_data);
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "⚠️ MQTT_EVENT_DISCONNECTED");
        mqtt_connected = false;
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "❌ MQTT_EVENT_ERROR");

        if (event->error_handle) {
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "   Tipo: TCP_TRANSPORT");
                ESP_LOGE(TAG, "   ESP-ERR: 0x%x (%d)", event->error_handle->esp_tls_last_esp_err, event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "   TLS stack err: 0x%x", event->error_handle->esp_tls_stack_err);
                ESP_LOGE(TAG, "   Cert verify: 0x%x", event->error_handle->esp_tls_cert_verify_flags);
                
                // Decodificar erro ESP-TLS
                if (event->error_handle->esp_tls_last_esp_err == 0x8006) {
                    ESP_LOGE(TAG, "   ⚠️ ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT");
                    ESP_LOGE(TAG, "   Causa: Timeout ao conectar TLS - verifique:");
                    ESP_LOGE(TAG, "   • Conexão de internet está funcionando?");
                    ESP_LOGE(TAG, "   • Porta 8883 está acessível?");
                    ESP_LOGE(TAG, "   • Endpoint AWS IoT está correto?");
                }
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "   CONNECTION_REFUSED: Código %d",
                         event->error_handle->connect_return_code);
            }
        }

        ESP_LOGE(TAG, "🔎 Possíveis causas:");
        ESP_LOGE(TAG, "   • Client ID diferente do permitido pela policy");
        ESP_LOGE(TAG, "   • Certificado não anexado ao Thing");
        ESP_LOGE(TAG, "   • Certificado inativo ou expirado");
        ESP_LOGE(TAG, "   • Endpoint incorreto");
        ESP_LOGE(TAG, "   • Subscribe ou Publish em tópico não autorizado");

        mqtt_connected = false;
        break;

    default:
        ESP_LOGI(TAG, "🔔 Evento MQTT ID: %d", event_id);
        break;
    }
}

void mqtt_manager_set_custom_handler(mqtt_custom_event_handler_t handler)
{
    custom_handler = handler;
    ESP_LOGI(TAG, "Handler customizado registrado");
}

void mqtt_manager_start(const char *endpoint, const char *client_id,
                       const uint8_t *root_ca, const uint8_t *device_cert, const uint8_t *device_key,
                       esp_mqtt_client_handle_t *out_client)
{
    char mqtt_url[256];
    snprintf(mqtt_url, sizeof(mqtt_url), "mqtts://%s:8883", endpoint);

    ESP_LOGI(TAG, "══════════════════════════════════════════════");
    ESP_LOGI(TAG, "            CONFIGURANDO MQTT AWS IoT          ");
    ESP_LOGI(TAG, "══════════════════════════════════════════════");
    ESP_LOGI(TAG, "🌐 URL: %s", mqtt_url);
    ESP_LOGI(TAG, "🔐 Client ID: %s", client_id);
    ESP_LOGI(TAG, "🔑 Root CA: %p", root_ca);
    ESP_LOGI(TAG, "🔑 Cert:    %p", device_cert);
    ESP_LOGI(TAG, "🔑 Key:     %p", device_key);
    
    // Log tamanho dos certificados para debug
    ESP_LOGI(TAG, "📏 Tamanho Root CA: ~%d bytes", strlen((const char *)root_ca));
    ESP_LOGI(TAG, "📏 Tamanho Cert: ~%d bytes", strlen((const char *)device_cert));
    ESP_LOGI(TAG, "📏 Tamanho Key: ~%d bytes", strlen((const char *)device_key));

    if (!root_ca || !device_cert || !device_key) {
        ESP_LOGE(TAG, "❌ Certificados ausentes! MQTT NÃO pode iniciar.");
        *out_client = NULL;
        return;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = mqtt_url,
            .verification = {
                .certificate = (const char *)root_ca,
                .use_global_ca_store = false,
            },
        },
        .credentials = {
            .authentication = {
                .certificate = (const char *)device_cert,
                .key = (const char *)device_key,
            },
            .client_id = client_id,
        },
        .session = {
            .keepalive = 120,
            .disable_clean_session = false,
        },
        .network = {
            .timeout_ms = 10000,  // Timeout de 10 segundos
            .refresh_connection_after_ms = 0,
            .disable_auto_reconnect = false,
        },
        .buffer = {
            .size = 2048,
            .out_size = 2048,
        }
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "❌ Falha ao inicializar cliente MQTT");
        *out_client = NULL;
        return;
    }

    ESP_LOGI(TAG, "✅ Cliente MQTT inicializado");
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));

    ESP_LOGI(TAG, "🚀 Iniciando cliente MQTT...");
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));

    *out_client = client;
}

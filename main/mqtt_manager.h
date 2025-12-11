#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "mqtt_client.h"

// Tipo de callback para eventos MQTT personalizados
typedef void (*mqtt_custom_event_handler_t)(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

void mqtt_manager_start(const char *endpoint, const char *client_id,
                       const uint8_t *root_ca, const uint8_t *device_cert, const uint8_t *device_key,
                       esp_mqtt_client_handle_t *out_client);

void mqtt_manager_set_custom_handler(mqtt_custom_event_handler_t handler);

extern bool mqtt_connected;

#endif // MQTT_MANAGER_H

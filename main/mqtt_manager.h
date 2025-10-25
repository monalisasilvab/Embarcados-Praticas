#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "mqtt_client.h"

void mqtt_manager_start(const char *endpoint, const char *client_id,
                       const uint8_t *root_ca, const uint8_t *device_cert, const uint8_t *device_key,
                       esp_mqtt_client_handle_t *out_client);

extern bool mqtt_connected;

#endif // MQTT_MANAGER_H

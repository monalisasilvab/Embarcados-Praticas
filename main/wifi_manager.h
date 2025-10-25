#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"

void wifi_manager_init(const char *ssid, const char *password, wifi_auth_mode_t authmode);

#endif // WIFI_MANAGER_H

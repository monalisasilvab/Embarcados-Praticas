idf_component_register(SRCS "main.c" "wifi_manager.c" "mqtt_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_wifi esp_event esp_netif mqtt esp_timer esp_driver_gpio
                    EMBED_TXTFILES certs/AmazonRootCA1.pem
                                  certs/21799d82cb8e53178d50791a8bce841ab0132bdc3596a4350b798c0b5dc0925a-certificate.pem.crt
                                  certs/21799d82cb8e53178d50791a8bce841ab0132bdc3596a4350b798c0b5dc0925a-private.pem.key)

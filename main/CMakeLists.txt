idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_wifi esp_event esp_netif mqtt lwip
                    EMBED_TXTFILES certs/AmazonRootCA1.pem
                                  certs/certificate.pem.crt
                                  certs/private.pem.key)

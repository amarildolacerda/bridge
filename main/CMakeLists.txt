idf_component_register(SRC_DIRS "."
                       PRIV_INCLUDE_DIRS "."
                        PRIV_REQUIRES esp_rainmaker esp_http_server json nvs_flash esp_timer esp_event esp_netif freertos log esp_system heap lwip rmaker_app_network mdns)

target_add_binary_data(${COMPONENT_LIB} "web/dashboard.html" TEXT)
target_add_binary_data(${COMPONENT_LIB} "web/dashboard.css" TEXT)

# Red: WiFi, cliente TCP/TLS, servidor web, mDNS y OTA.
#
# Es el grupo PESADO — arrastra esp_wifi, esp_http_server, esp-tls, mbedtls y
# mDNS. Tenerlo separado es lo que permite que un test de almacenamiento o de
# I2C no compile nada de esto.
#
# plat_net_client reproduce dos cosas que la WiFiClient de Arduino si hacia y
# que el porte perdio: available() sobre los bytes realmente pendientes (exige
# CONFIG_LWIP_SO_RCVBUF) y una espera que evitaba un LINK LOST fantasma.
idf_component_register(
  SRCS
    "src/plat_wifi.cpp"
    "src/plat_net_client.cpp"
    "src/plat_webserver.cpp"
    "src/plat_update.cpp"
    "src/plat_mdns.cpp"
  INCLUDE_DIRS "include"
  # nvs_flash: plat_wifi.cpp lo necesita porque el driver de WiFi guarda ahi su
  # configuracion y su calibracion de PHY.
  REQUIRES incunest_plat_core nvs_flash esp_wifi esp_netif esp_event esp_http_server
           app_update esp-tls lwip mbedtls espressif__mdns
)

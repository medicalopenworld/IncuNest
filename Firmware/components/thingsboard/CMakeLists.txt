cmake_minimum_required(VERSION 3.12)

set(srcs
    src/Arduino_HTTP_Client.cpp
    src/Arduino_MQTT_Client.cpp
    src/Callback_Watchdog.cpp
    src/Arduino_ESP32_Updater.cpp
    src/Arduino_ESP8266_Updater.cpp
    src/Espressif_Updater.cpp
    src/SDCard_Updater.cpp
    src/Espressif_MQTT_Client.cpp
    src/HashGenerator.cpp
    src/Helper.cpp
    src/OTA_Update_Callback.cpp
    src/Provision_Callback.cpp
    src/RPC_Request_Callback.cpp
    src/Telemetry.cpp
)

set(dependencies
    mqtt
    mbedtls
    # PARCHE INCUNEST (ver PATCHES.md, parche 3): Configuration.h decide
    # THINGSBOARD_USE_ESP_PARTITION con __has_include(<esp_ota_ops.h>). Sin
    # app_update en los REQUIRES, dentro de este componente la cabecera no se
    # ve y Espressif_Updater.cpp compila VACIO, mientras que la aplicacion (que
    # si tiene app_update) ve la clase completa y acaba con referencias sin
    # definir al enlazar. Va como dependencia publica porque la decision se
    # toma tambien en las cabeceras que incluye la aplicacion.
    app_update
    esp_app_format
)

set(private_dependencies
    esp_timer
)

if(ESP_PLATFORM)
	# Build ThingsBoard Arduino SDK as an ESP-IDF component
	idf_component_register(
		SRCS ${srcs}
		INCLUDE_DIRS "src"
        PRIV_INCLUDE_DIRS "src"
		REQUIRES ${dependencies}
		PRIV_REQUIRES ${private_dependencies}
	)
	return()
endif()

project(ThingsBoardClientSDK VERSION 0.13.0)

# ESP-IDF compila con -Werror y esta es una SDK de terceros que nunca se construyo con
# esa politica: sus avisos preexistentes (orden de inicializadores, campos sin
# inicializar) no son del porte y arreglarlos seria separarse mas de upstream.
# Se rebajan a aviso SOLO en este componente.
target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-error=reorder
  -Wno-error=missing-field-initializers
  -Wno-error=unused-variable
  -Wno-error=format=
  -Wno-error=format-truncation
)

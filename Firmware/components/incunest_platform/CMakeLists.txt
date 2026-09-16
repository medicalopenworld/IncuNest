# Agregador de la capa de plataforma. NO tiene fuentes propias.
#
# ================== POR QUE ESTE COMPONENTE SIGUE EXISTIENDO ==================
#
# Hasta 2026-09-16 esto era UN SOLO componente con todo dentro y 21 REQUIRES.
# Funcionaba, pero tenia un coste concreto: cualquiera que quisiera una sola
# pieza se llevaba el resto. Al escribir el primer test_app de contrato
# (motherBoard/test_apps/plat_fs_test) resulto que un test del SISTEMA DE
# FICHEROS acababa compilando mDNS, WiFi, HTTP y mbedTLS.
#
# Ahora hay cuatro componentes de verdad, agrupados por PESO DE DEPENDENCIAS y
# no uno por fichero, que seria mucho ruido para el mismo beneficio:
#
#   incunest_plat_core     String, Print, millis(), num, tipos   (esp_timer)
#   incunest_plat_hw       GPIO, PWM, I2C, SPI, UART             (esp_driver_*)
#   incunest_plat_storage  LittleFS y NVS  <- DATOS DE PACIENTE  (littlefs, nvs)
#   incunest_plat_net      WiFi, TLS, webserver, mDNS, OTA       (el grupo pesado)
#
# El reparto sale del arbol de dependencias real entre las capas, que se
# comprobo antes de partir: cuelga casi todo de plat_string y plat_print, y NO
# HAY CICLOS.
#
# ESTE AGREGADOR EXISTE PARA NO TOCAR EL FIRMWARE. Hay 157 inclusiones de
# `platform/*.h` repartidas por las dos placas, y sus CMakeLists piden
# `incunest_platform`. Requiriendo aqui los cuatro, los directorios de include
# se propagan y todo eso sigue compilando sin cambiar ni una linea. El reparto
# es de CONSTRUCCION, no de codigo: cero cambios de comportamiento.
#
# COMO USARLO EN CODIGO NUEVO: pedir el componente concreto, no este. Este es
# la compatibilidad con lo que ya existe; depender de el vuelve a arrastrarlo
# todo, que es justo lo que se acaba de arreglar.
idf_component_register(
  INCLUDE_DIRS ""
  REQUIRES incunest_plat_core incunest_plat_hw incunest_plat_storage
           incunest_plat_net
)

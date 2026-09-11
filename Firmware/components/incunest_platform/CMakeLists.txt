# Componente "platform": los primitivos que antes ponia el framework Arduino,
# reimplementados sobre ESP-IDF puro. No depende de arduino-esp32 ni lo emula:
# cada pieza es una envoltura fina sobre la API nativa (esp_timer, driver/gpio,
# esp_driver_ledc, esp_driver_i2c, nvs_flash, esp_log). En IDF 6 el antiguo
# componente "driver" esta partido en esp_driver_*, de ahi la lista de abajo.
#
# Por que existe en vez de convertir cada llamada a la API de IDF en su sitio:
# el firmware es de un equipo medico con unidades en campo. Concentrar la
# traduccion en un unico punto revisable deja el resto del porte como cambios
# mecanicos y auditables, en vez de 62k lineas de reescritura dispersa.
idf_component_register(
  SRCS
    "src/plat_time.c"
    "src/plat_gpio.c"
    "src/plat_pwm.c"
    "src/plat_i2c.cpp"
    "src/plat_nvs.cpp"
    "src/plat_string.cpp"
    "src/plat_num.c"
    "src/plat_spi.cpp"
    "src/plat_fs.cpp"
    "src/plat_esp.cpp"
  INCLUDE_DIRS "include"
  REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_driver_spi esp_adc esp_timer nvs_flash joltwallet__littlefs vfs esp_system spi_flash
)

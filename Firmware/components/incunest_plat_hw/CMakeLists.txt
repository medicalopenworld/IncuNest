# Perifericos del chip: GPIO, PWM (LEDC), I2C, SPI y UART.
#
# En IDF 6 el antiguo componente "driver" esta partido en esp_driver_*, de ahi
# la lista. No incluye nada de red ni de almacenamiento.
#
# plat_i2c reproduce a proposito la semantica de mutex de la TwoWire de
# Arduino: el bloqueo abarca la transaccion completa, repeated-start incluido.
# Perder eso fue un fallo real del porte (91c578a).
idf_component_register(
  SRCS
    "src/plat_gpio.c"
    "src/plat_pwm.c"
    "src/plat_i2c.cpp"
    "src/plat_spi.cpp"
    "src/plat_uart.cpp"
  INCLUDE_DIRS "include"
  REQUIRES incunest_plat_core esp_driver_gpio esp_driver_ledc esp_driver_i2c
           esp_driver_spi esp_driver_uart esp_adc
)

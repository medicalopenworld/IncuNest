# incunest_afe4490 (frontal analogico de SpO2) como componente de ESP-IDF.
#
# Copia local de https://github.com/medicalopenworld/incunest_afe4490 pineada
# en el commit 4e0dd91 (v0.81.0), el mismo que fijaba platformio.ini.
#
# El delta frente a upstream son ~28 puntos de llamada, todos marcados con
# "PARCHE INCUNEST": <Arduino.h> y <SPI.h> pasan a la capa de plataforma,
# digitalWrite/pinMode a pin_write/pin_mode, delay a delay_ms y las tres
# trazas de Serial a ESP_LOG. La logica de senal no se toca.
#
# OJO AL MANTENIMIENTO: esta libreria vive en OTRO REPOSITORIO con otro
# responsable, y platformio.ini documentaba por que esta pineada a un commit
# (en v0.69 cambio ppg_disp de int32_t a float y el firmware siguio
# compilando, truncando cada muestra a 0 — una traza plana en un monitor de
# paciente). Al subir el pin hay que rehacer estos parches y volver a mirar
# el static_assert de src/drivers/SPO2.h.
#
# PENDIENTE DE BANCO: la cadencia real de las muestras de PPG con el SPI de
# ESP-IDF. Ver la nota de components/incunest_platform/include/platform/plat_spi.h.
idf_component_register(
  SRCS "src/incunest_afe4490.cpp"
  INCLUDE_DIRS "src"
  REQUIRES incunest_platform esp_timer
)

# ESP-IDF compila con -Werror y esta es una libreria de otro repositorio que nunca se construyo con
# esa politica: sus avisos preexistentes (orden de inicializadores, campos sin
# inicializar) no son del porte y arreglarlos seria separarse mas de upstream.
# Se rebajan a aviso SOLO en este componente.
target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-error=reorder
  -Wno-error=missing-field-initializers
  -Wno-error=unused-variable
  -Wno-error=format=
  -Wno-error=format-truncation
  -Wno-error=volatile
  -Wno-error=deprecated
)

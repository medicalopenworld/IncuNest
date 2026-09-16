# Pantalla tactil GT911 (TAMC_GT911, version ya corregida por el equipo).
#
# Antes vivia en Display_HMI/lib/TAMC_GT911_Fixed/, que es como PlatformIO
# recoge las librerias locales. Pasa a componente de ESP-IDF sin cambiar su
# logica: el delta son los puntos marcados "PARCHE INCUNEST" —Wire -> I2cBus,
# Arduino.h -> capa de plataforma, digitalWrite/pinMode/delay renombrados.
#
# La copia original en Display_HMI/lib/ queda de momento (no entra en el build
# de IDF); borrarla es un commit aparte.
idf_component_register(
  SRCS "src/TAMC_GT911.cpp"
  INCLUDE_DIRS "src"
  REQUIRES incunest_platform
)

target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-error=reorder
  -Wno-error=missing-field-initializers
  -Wno-error=volatile
  -Wno-error=uninitialized
  -Wno-error=maybe-uninitialized
  -Wno-error=unused-variable
  -Wno-error=sign-compare
)

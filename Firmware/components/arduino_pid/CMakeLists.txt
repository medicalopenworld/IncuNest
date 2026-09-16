# Arduino-PID-Library como componente de ESP-IDF.
#
# Copia local de https://github.com/br3ttb/Arduino-PID-Library pineada en el
# commit 524a4268fc01e6ea397e7fc5b5d820741e9b662f, el mismo que fijaba
# platformio.ini. El delta frente a upstream es UNA linea por fichero: el
# #include "Arduino.h" pasa a "platform/plat_time.h", porque lo unico que la
# libreria necesitaba de Arduino era millis() — y ese nombre lo conserva
# nuestra capa de plataforma. Los dos cambios llevan el marcador
# "PARCHE INCUNEST".
#
# Se vendoriza en vez de reescribirla porque es el lazo de control del
# calefactor de un equipo medico: mantener el algoritmo byte a byte, con su
# anti-windup y su tratamiento del cambio de modo, vale mas que tener 200
# lineas propias.
idf_component_register(
  SRCS "src/PID_v1.cpp"
  INCLUDE_DIRS "src"
  REQUIRES incunest_platform
)

# ESP-IDF compila con -Werror y esta es una libreria de terceros que nunca se construyo con
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

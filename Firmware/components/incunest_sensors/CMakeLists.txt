# Librerias de sensor de terceros, vendorizadas para el porte a ESP-IDF.
#
# POR QUE VENDORIZAR EN VEZ DE REESCRIBIR: aqui dentro estan las formulas de
# conversion, los CRC y las secuencias de comando de los sensores que miden la
# temperatura del aire y de la piel — lo que gobierna el calefactor. Reescribir
# esa matematica a mano seria meter riesgo clinico a cambio de nada. Se les
# cambia SOLO el transporte y se deja el algoritmo intacto.
#
# El delta frente a upstream va marcado con "PARCHE INCUNEST" en el codigo y
# se resume en VENDORED.md: Wire -> I2cBus, Arduino.h -> capa de plataforma,
# digitalWrite/pinMode/delay -> pin_write/pin_mode/delay_ms. Nada mas.
#
# Todas se copiaron de las versiones que platformio.ini ya resolvia, no de la
# ultima de cada proyecto: el porte no cambia de version de ninguna libreria.
# NOTA sobre arduino_filters: se aportan SOLO sus cabeceras. El paquete trae
# ademas "Arduino Helpers" (botones, registros de desplazamiento, PrintStream)
# que este firmware no usa y que arrastraba dependencias de Arduino; sus .cpp
# quedan fuera del build. Lo que se usa —Timer<micros> y el Butterworth de
# sexto orden de sensors_module.cpp— son plantillas de cabecera.
idf_component_register(
  SRCS
    "vendor/adafruit_busio/Adafruit_BusIO_Register.cpp"
    "vendor/adafruit_busio/Adafruit_GenericDevice.cpp"
    "vendor/adafruit_busio/Adafruit_I2CDevice.cpp"
    "vendor/adafruit_busio/Adafruit_SPIDevice.cpp"
    "vendor/adafruit_sensor/Adafruit_Sensor.cpp"
    "vendor/adafruit_sht4x/Adafruit_SHT4x.cpp"
    "vendor/ina3221/Beastdevices_INA3221.cpp"
    "vendor/rotary_encoder/RotaryEncoder.cpp"
    "vendor/sensirion_core/SensirionCrc.cpp"
    "vendor/sensirion_core/SensirionErrors.cpp"
    "vendor/sensirion_core/SensirionI2CCommunication.cpp"
    "vendor/sensirion_core/SensirionI2CTxFrame.cpp"
    "vendor/sensirion_core/SensirionRxFrame.cpp"
    "vendor/sensirion_sts3x/SensirionI2cSts3x.cpp"
    "vendor/sparkfun_shtc3/SparkFun_SHTC3.cpp"
    "vendor/tca9555/TCA9555.cpp"
  INCLUDE_DIRS
    "vendor/adafruit_busio"
    "vendor/adafruit_sensor"
    "vendor/adafruit_sht4x"
    "vendor/arduino_filters"
    "vendor/arduino_filters/AH"
    "vendor/arduino_filters/AH/Containers"
    "vendor/arduino_filters/AH/Debug"
    "vendor/arduino_filters/AH/Error"
    "vendor/arduino_filters/AH/Filters"
    "vendor/arduino_filters/AH/Hardware"
    "vendor/arduino_filters/AH/Hardware/ExtendedInputOutput"
    "vendor/arduino_filters/AH/Hardware/LEDs"
    "vendor/arduino_filters/AH/Math"
    "vendor/arduino_filters/AH/PrintStream"
    "vendor/arduino_filters/AH/STL/Fallback/backward"
    "vendor/arduino_filters/AH/STL/Fallback/bits"
    "vendor/arduino_filters/AH/STL/Fallback/debug"
    "vendor/arduino_filters/AH/STL/Fallback/ext"
    "vendor/arduino_filters/AH/Settings"
    "vendor/arduino_filters/AH/Teensy"
    "vendor/arduino_filters/AH/Timing"
    "vendor/arduino_filters/AH/Types"
    "vendor/arduino_filters/Filters"
    "vendor/ina3221"
    "vendor/rotary_encoder"
    "vendor/sensirion_core"
    "vendor/sensirion_sts3x"
    "vendor/sparkfun_shtc3"
    "vendor/tca9555"
  REQUIRES incunest_platform
)

target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-error=reorder
  -Wno-error=missing-field-initializers
  -Wno-error=unused-variable
  -Wno-error=unused-but-set-variable
  -Wno-error=format=
  -Wno-error=format-truncation
  -Wno-error=volatile
  -Wno-error=deprecated
  -Wno-error=sign-compare
  -Wno-error=attributes
  -Wno-error=switch
  -Wno-error=maybe-uninitialized
)

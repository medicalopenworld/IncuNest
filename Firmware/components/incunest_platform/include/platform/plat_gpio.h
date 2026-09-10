#pragma once

// GPIO digital y ADC sobre driver/gpio y esp_adc (ESP-IDF puro).
//
// A diferencia de plat_time.h, aqui SI se renombran las funciones: son ~60
// puntos de llamada en total (contra 390 de millis()), y los modos de Arduino
// (INPUT/OUTPUT/INPUT_PULLUP) son macros suyas. Mantener esos nombres obligaria
// a redefinir el vocabulario de Arduino, que es justo lo que este porte quita.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PIN_MODE_INPUT = 0,
  PIN_MODE_OUTPUT,
  PIN_MODE_INPUT_PULLUP,
  PIN_MODE_INPUT_PULLDOWN,
  PIN_MODE_OUTPUT_OPEN_DRAIN,
} pin_mode_t;

void pin_mode(uint8_t pin, pin_mode_t mode);
void pin_write(uint8_t pin, bool level);
bool pin_read(uint8_t pin);

// Lectura del ADC en milivoltios, con la curva de calibracion de fabrica del
// chip (esp_adc_cal). Equivale a analogReadMilliVolts() y, como ella, admite
// que se le pase el numero de GPIO: la traduccion a unidad+canal se hace
// dentro. Devuelve 0 si el pin no tiene ADC o la calibracion no esta.
uint32_t adc_read_mv(uint8_t pin);

#ifdef __cplusplus
}
#endif

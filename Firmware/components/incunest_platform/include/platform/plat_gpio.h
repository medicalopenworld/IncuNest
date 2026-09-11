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

// HIGH y LOW se conservan con su valor de Arduino (1 y 0). No son modos: el
// codigo los usa sueltos en comparaciones del estilo
// `if (pin_read(p) == HIGH)`, y mantenerlos evita tocar esos sitios sin ganar
// nada. Son dos constantes, no una dependencia.
#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif

// INPUT/OUTPUT con sus valores de Arduino. Aqui NO son el modo de un pin del
// ESP32 (para eso esta pin_mode_t): se conservan porque el codigo los usa
// tambien como valor de direccion de los expansores de E/S por I2C
// (TCA9535 en initHardware.cpp) y en la libreria vendorizada TCA9555.
#ifndef OUTPUT
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05
#define INPUT_PULLDOWN 0x09
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

// --- Interrupciones de GPIO -------------------------------------------------
// Sustituyen a attachInterrupt()/detachInterrupt() de Arduino. El manejador
// tiene la misma firma que alli —void(void), sin argumento— para que los dos
// puntos de llamada del firmware (el DRDY del AFE4490 y el encoder en
// initHardware) no cambien de forma; por dentro se registra en el servicio de
// ISR de ESP-IDF.
//
// Igual que en Arduino, el manejador corre EN CONTEXTO DE INTERRUPCION: tiene
// que llevar IRAM_ATTR y no puede bloquear, reservar memoria ni loguear.
typedef enum {
  PIN_INT_RISING = 0,
  PIN_INT_FALLING,
  PIN_INT_CHANGE,
  PIN_INT_LOW_LEVEL,
  PIN_INT_HIGH_LEVEL,
} pin_int_mode_t;

void pin_attach_interrupt(uint8_t pin, void (*handler)(void),
                          pin_int_mode_t mode);
void pin_detach_interrupt(uint8_t pin);

#ifdef __cplusplus
}
#endif

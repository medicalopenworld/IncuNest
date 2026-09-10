#pragma once

// PWM sobre driver/ledc (ESP-IDF puro).
//
// EL MODELO DE CANALES SE CONSERVA A PROPOSITO. El firmware esta escrito
// alrededor de canales fijos (HEATER_PWM_CHANNEL, BUZZER_PWM_CHANNEL...,
// board.h:163-172) y de una correspondencia canal->timer que board.h:167
// documenta explicitamente: el ventilador se movio al canal 7 para que su
// frecuencia no pisara los 400 Hz del CALEFACTOR. Esa correspondencia era una
// regla interna de arduino-esp32:
//
//     timer = (canal / 2) % 4        (esp32-hal-ledc.c de Arduino 2.x)
//
// Se reproduce aqui literalmente. Cambiarla por una asignacion "mejor" de
// timers alteraria en silencio la frecuencia real de un actuador termico en un
// equipo medico, que es exactamente lo que un porte no debe hacer.
//
// Reparto que resulta con los canales que usa la placa (todos a 8 bits):
//   timer 0 <- ch0 retroiluminacion 400 Hz, ch1 zumbador 400 Hz
//   timer 1 <- ch2 CALEFACTOR 400 Hz
//   timer 2 <- ch4 fototerapia 10 kHz   (ch5 humidificador comparte timer)
//   timer 3 <- ch6 fan_ctl 400 Hz, ch7 ventilador 400 Hz

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configura frecuencia y resolucion del timer que le toca al canal.
// Devuelve la frecuencia realmente conseguida, o 0 si fallo (igual que
// ledcSetup, cuyo valor de retorno el firmware no comprueba en ningun sitio).
uint32_t pwm_setup(uint8_t channel, uint32_t freq_hz, uint8_t resolution_bits);

// Enruta el canal a un pin. Debe ir despues de pwm_setup() para ese canal.
void pwm_attach(uint8_t pin, uint8_t channel);

// Duty en cuentas de la resolucion configurada (0..2^bits-1).
void pwm_write(uint8_t channel, uint32_t duty);

#ifdef __cplusplus
}
#endif

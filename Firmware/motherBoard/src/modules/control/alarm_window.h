#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Aritmetica de la ventana de estabilizacion de alarmas, extraida de
// security.cpp para poder verificarla en el entorno nativo: es donde vivio un
// bug de offset que dejaba la ventana contando desde el arranque en vez de
// desde la activacion de la terapia.
//
// Todo es uint32_t a proposito. El reloj es millis(), que desborda a los ~49
// dias, y la resta modular de uint32_t sigue dando el intervalo correcto al
// cruzar ese punto. Mezclar tipos con signo aqui es lo que produce ventanas
// absurdas justo despues del desbordamiento.

// Instante que hay que guardar como inicio de una ventana que, en `now_ms`, ya
// lleva `already_elapsed_ms` consumidos. Con already_elapsed_ms == 0 devuelve
// `now_ms` (ventana completa por delante); con already_elapsed_ms igual o
// mayor que la ventana, esta nace cerrada.
uint32_t alarm_window_start(uint32_t now_ms, uint32_t already_elapsed_ms);

// Lo que le queda a la ventana en `now_ms`, o 0 si ya se cerro.
uint32_t alarm_window_remaining_ms(uint32_t start_ms, uint32_t window_ms,
                                   uint32_t now_ms);

#ifdef __cplusplus
}
#endif

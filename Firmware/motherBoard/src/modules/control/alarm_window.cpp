#include "alarm_window.h"

uint32_t alarm_window_start(uint32_t now_ms, uint32_t already_elapsed_ms) {
  // Restar hacia atras desde `now_ms`, no partir de 0. Partir de 0 hacia que
  // la ventana se contase desde el origen del reloj: con la terapia iniciada
  // mas de una ventana despues de encender el equipo, nacia ya cerrada.
  return now_ms - already_elapsed_ms;
}

uint32_t alarm_window_remaining_ms(uint32_t start_ms, uint32_t window_ms,
                                   uint32_t now_ms) {
  const uint32_t elapsed = now_ms - start_ms;
  return (elapsed < window_ms) ? (window_ms - elapsed) : 0u;
}

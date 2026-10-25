#pragma once

// Espera progresiva de reintento y limitador de log, sin hardware.
//
// Los usa sensors_Task para el cargador BQ25730: si en el arranque el chip
// contesta pero la configuracion falla (_initialized == false), no carga la
// bateria (ChargeCurrent de fabrica = 0) y la red no se vigila. Hay que
// reintentar, pero ni cada 5 s para siempre (martilleo del bus I2C) ni una
// sola vez. Tests: motherBoard/test/test_retry_backoff.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Espera entre intentos: 5 s, 10 s, 20 s ... hasta 5 min.
#define BACKOFF_MIN_MS 5000u
#define BACKOFF_MAX_MS 300000u

typedef struct {
  uint32_t delay_ms;  // espera aplicada tras el ultimo fallo (0 = sin fallos)
  uint32_t next_ms;   // instante del proximo intento permitido
} Backoff;

void backoff_init(Backoff *b);

// true si toca intentar. Sin fallos pendientes siempre es true.
bool backoff_due(const Backoff *b, uint32_t now_ms);

// El intento fallo: programa el siguiente con el doble de espera (tope
// BACKOFF_MAX_MS).
void backoff_failed(Backoff *b, uint32_t now_ms);

// El intento salio bien: se olvida la espera.
void backoff_ok(Backoff *b);

// Limitador de log: un mensaje por episodio de fallo y despues uno cada
// LOG_LIMITER_INTERVAL_MS, para que un fallo persistente no llene el registro.
#define LOG_LIMITER_INTERVAL_MS 300000u

typedef struct {
  uint32_t last_ms;
  bool logged;
} LogLimiter;

void log_limiter_init(LogLimiter *l);

// true si este mensaje se puede escribir ahora (y lo anota).
bool log_limiter_allow(LogLimiter *l, uint32_t now_ms);

// El episodio termino: el proximo fallo vuelve a loguearse enseguida.
void log_limiter_reset(LogLimiter *l);

#ifdef __cplusplus
}
#endif

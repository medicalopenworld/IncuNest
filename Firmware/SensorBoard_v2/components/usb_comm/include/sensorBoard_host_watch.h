#pragma once
/* Vigilante de host USB: decide cuándo el SensorBoard debe reiniciarse para
 * forzar la re-enumeración tras perder a la motherboard.
 *
 * Función pura de (estado, señales, reloj) para poder probarla con Unity sin
 * USB. Las señales las aporta el transporte:
 *  - dtr:       el host tiene el puerto abierto (SET_CONTROL_LINE_STATE).
 *  - bus_ready: TinyUSB montado y NO suspendido (tud_ready()). Es la única
 *               señal que cae cuando se desenchufa el cable o el host se
 *               reinicia sin cortar VBUS: en ese caso no llega ningún
 *               cambio de DTR, solo cesan los SOF y el DCD reporta SUSPEND.
 *
 * "Host presente" = dtr && bus_ready. Se pide reinicio cuando el host se
 * pierde durante SB_HOST_LOST_RESTART_MS HABIÉNDOLO TENIDO antes: una placa
 * que arranca antes que la motherboard, o que vive sola, espera sin reiniciar.
 * Recuperar el host antes del plazo desarma el temporizador; una pérdida
 * posterior vuelve a contar desde cero. */
#include <stdbool.h>
#include <stdint.h>

#define SB_HOST_LOST_RESTART_MS 15000u

typedef struct {
    bool ever_seen;      /* alguna vez hubo host presente */
    bool lost;           /* host perdido y temporizador armado */
    uint32_t lost_at_ms; /* instante de la pérdida (válido si lost) */
} sb_host_watch_t;

void sb_host_watch_init(sb_host_watch_t *w);

/* Actualiza el estado con las señales actuales. Devuelve true cuando toca
 * reiniciar. Resta sin signo sobre now_ms: sobrevive al vuelco de uint32. */
bool sb_host_watch_update(sb_host_watch_t *w, bool dtr, bool bus_ready, uint32_t now_ms);

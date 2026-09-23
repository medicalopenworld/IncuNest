/* Política de orientación del enlace USB (tolerancia a D+/D- cruzados).
 *
 * Módulo PURO: sin TinyUSB ni registros. Decide CUÁNDO intercambiar D+/D- a
 * partir de tres entradas — "¿host activo?" (SETUP recibido o configurado),
 * "¿bus reset visto desde el último tick?" y el instante actual en ms — y el
 * caller (usb_comm) ejecuta el intercambio en el PHY.
 *
 * Solo intercambia con EVIDENCIA de host (un bus reset) y una sola vez por
 * evidencia: sin host no se mueve, y tras intercambiar espera quieto a que el
 * host vuelva a hacer reset. Alternar a ciegas cada T engancha en fase con la
 * recuperación del puerto de la pila host de la motherboard (ESP-IDF 4.4) y
 * cae siempre en la ventana equivocada (banco 2026-09-03).
 * Interno de usb_comm; expuesto en include/ solo para los tests Unity. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SB_ORIENT_NONE = 0, /* nada que hacer */
    SB_ORIENT_SWAP,     /* el caller debe aplicar st->swapped al PHY y re-attach */
} sb_orient_action_t;

typedef struct {
    uint32_t timeout_ms;  /* 0 = política desactivada */
    uint32_t deadline_ms; /* instante en que vence el plazo (si armed) */
    uint32_t swap_count;  /* intercambios acumulados (diagnóstico) */
    bool armed;           /* hay un reset sin respuesta del host pendiente de plazo */
    bool swapped;         /* orientación actual: false = normal, true = D+/D- cruzados */
} sb_usb_orient_t;

/* Orientación normal, sin plazo armado (no se mueve hasta ver un reset). */
void sb_usb_orient_init(sb_usb_orient_t *st, uint32_t timeout_ms);

/* Llamar periódicamente (cadencia << timeout).
 * - host_active: desarma y nunca intercambia.
 * - bus_reset_seen: arma el plazo a now+timeout si no estaba armado (resets
 *   posteriores NO lo extienden).
 * - Armado y vencido sin host: conmuta `swapped`, desarma y devuelve
 *   SB_ORIENT_SWAP exactamente una vez; no vuelve a actuar sin un nuevo reset.
 * Robusta al desbordamiento de now_ms (aritmética uint32 con signo). */
sb_orient_action_t sb_usb_orient_tick(sb_usb_orient_t *st, bool host_active, bool bus_reset_seen,
                                      uint32_t now_ms);

bool sb_usb_orient_is_swapped(const sb_usb_orient_t *st);
uint32_t sb_usb_orient_swap_count(const sb_usb_orient_t *st);

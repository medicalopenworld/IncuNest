/* Política de orientación del enlace USB (tolerancia a D+/D- cruzados).
 *
 * Módulo PURO: sin TinyUSB ni registros. Decide CUÁNDO intercambiar D+/D- a
 * partir de dos entradas — "¿enlace montado?" (tud_mounted) y el instante
 * actual en ms — y el caller (usb_comm) ejecuta el intercambio en el PHY.
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
    uint32_t deadline_ms; /* instante en que vence el plazo sin enumeración */
    uint32_t swap_count;  /* intercambios acumulados (diagnóstico) */
    bool swapped;         /* orientación actual: false = normal, true = D+/D- cruzados */
} sb_usb_orient_t;

/* Orientación normal y plazo armado a now_ms + timeout_ms. */
void sb_usb_orient_init(sb_usb_orient_t *st, uint32_t timeout_ms, uint32_t now_ms);

/* Llamar periódicamente (cadencia << timeout). Con link_up rearma el plazo y
 * nunca intercambia. Sin link_up, al vencer el plazo conmuta `swapped`,
 * rearma y devuelve SB_ORIENT_SWAP exactamente una vez por vencimiento.
 * Robusta al desbordamiento de now_ms (aritmética uint32 con signo). */
sb_orient_action_t sb_usb_orient_tick(sb_usb_orient_t *st, bool link_up, uint32_t now_ms);

bool sb_usb_orient_is_swapped(const sb_usb_orient_t *st);
uint32_t sb_usb_orient_swap_count(const sb_usb_orient_t *st);

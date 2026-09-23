#include "sensorBoard_usb_orient.h"
#include <stddef.h>

/* true si now_ms ha alcanzado o superado deadline_ms, con aritmética uint32
 * con signo: robusta al desbordamiento (~49.7 días) mientras la distancia
 * entre ambos sea < 2^31 ms — siempre cierto con timeouts de segundos. */
static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

void sb_usb_orient_init(sb_usb_orient_t *st, uint32_t timeout_ms)
{
    if (st == NULL) {
        return;
    }
    st->timeout_ms = timeout_ms;
    st->deadline_ms = 0;
    st->swap_count = 0;
    st->armed = false;
    st->swapped = false;
}

sb_orient_action_t sb_usb_orient_tick(sb_usb_orient_t *st, bool host_active, bool bus_reset_seen,
                                      uint32_t now_ms)
{
    if (st == NULL || st->timeout_ms == 0) {
        return SB_ORIENT_NONE; /* desactivada: la orientación queda como esté */
    }

    if (host_active) {
        /* El host nos habla: la orientación actual es la correcta. */
        st->armed = false;
        return SB_ORIENT_NONE;
    }

    /* Evidencia de host: solo el PRIMER reset arma el plazo. Los siguientes
     * no lo extienden — un host que reintenta varios resets seguidos sobre un
     * dispositivo que no le responde no debe posponer el intercambio. */
    if (bus_reset_seen && !st->armed) {
        st->armed = true;
        st->deadline_ms = now_ms + st->timeout_ms;
    }

    if (!st->armed || !deadline_reached(now_ms, st->deadline_ms)) {
        return SB_ORIENT_NONE;
    }

    /* Plazo vencido sin que el host nos haya hablado: intercambiar UNA vez y
     * quedarse quieto hasta la siguiente evidencia. La orientación nueva se
     * mantiene el tiempo que el host tarde en recuperar su puerto. */
    st->swapped = !st->swapped;
    st->swap_count++;
    st->armed = false;
    return SB_ORIENT_SWAP;
}

bool sb_usb_orient_is_swapped(const sb_usb_orient_t *st)
{
    return (st != NULL) && st->swapped;
}

uint32_t sb_usb_orient_swap_count(const sb_usb_orient_t *st)
{
    return (st != NULL) ? st->swap_count : 0;
}

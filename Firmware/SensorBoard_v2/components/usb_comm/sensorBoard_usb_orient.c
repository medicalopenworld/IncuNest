#include "sensorBoard_usb_orient.h"
#include <stddef.h>

/* true si now_ms ha alcanzado o superado deadline_ms, con aritmética uint32
 * con signo: robusta al desbordamiento (~49.7 días) mientras la distancia
 * entre ambos sea < 2^31 ms — siempre cierto con timeouts de segundos. */
static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

void sb_usb_orient_init(sb_usb_orient_t *st, uint32_t timeout_ms, uint32_t now_ms)
{
    if (st == NULL) {
        return;
    }
    st->timeout_ms = timeout_ms;
    st->deadline_ms = now_ms + timeout_ms;
    st->swap_count = 0;
    st->swapped = false;
}

sb_orient_action_t sb_usb_orient_tick(sb_usb_orient_t *st, bool link_up, uint32_t now_ms)
{
    if (st == NULL || st->timeout_ms == 0) {
        return SB_ORIENT_NONE; /* desactivada: la orientación queda como esté */
    }

    if (link_up) {
        /* Enlace montado: la orientación actual es la correcta. Se rearma el
         * plazo para que una caída posterior cuente desde la caída. */
        st->deadline_ms = now_ms + st->timeout_ms;
        return SB_ORIENT_NONE;
    }

    if (!deadline_reached(now_ms, st->deadline_ms)) {
        return SB_ORIENT_NONE;
    }

    st->swapped = !st->swapped;
    st->swap_count++;
    st->deadline_ms = now_ms + st->timeout_ms;
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

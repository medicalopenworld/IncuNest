#include "sensorBoard_host_watch.h"

void sb_host_watch_init(sb_host_watch_t *w)
{
    w->ever_seen = false;
    w->lost = false;
    w->lost_at_ms = 0;
}

bool sb_host_watch_update(sb_host_watch_t *w, bool dtr, bool bus_ready, uint32_t now_ms)
{
    const bool present = dtr && bus_ready;

    if (present) {
        w->ever_seen = true;
        w->lost = false; /* recuperado antes del plazo: se desarma */
        return false;
    }
    if (!w->ever_seen) {
        return false; /* nunca hubo host: esperar sin reiniciar */
    }
    if (!w->lost) {
        w->lost = true; /* primera muestra sin host: arrancar el plazo */
        w->lost_at_ms = now_ms;
        return false;
    }
    /* Resta sin signo: correcta aunque now_ms haya dado la vuelta */
    return (uint32_t)(now_ms - w->lost_at_ms) >= SB_HOST_LOST_RESTART_MS;
}

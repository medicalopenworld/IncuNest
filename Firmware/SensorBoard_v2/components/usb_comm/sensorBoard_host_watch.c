#include "sensorBoard_host_watch.h"

void sb_host_watch_init(sb_host_watch_t *w)
{
    w->ever_seen = false;
    w->lost = false;
    w->lost_at_ms = 0;
}

bool sb_host_watch_update(sb_host_watch_t *w, bool dtr, bool bus_ready, uint32_t now_ms)
{
    (void)w;
    (void)dtr;
    (void)bus_ready;
    (void)now_ms;
    return false; /* stub: los tests deben fallar (red) */
}

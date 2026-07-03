#include "sb_door_logic.h"
#include <stdio.h>

void sb_door_fsm_init(sb_door_fsm_t *fsm)
{
    fsm->last_reported = -1; /* desconocido: la primera evaluación reporta */
}

sb_door_event_t sb_door_fsm_update(sb_door_fsm_t *fsm, int level, bool active_low)
{
    /* Normaliza: 1 = abierta, 0 = cerrada */
    int open = active_low ? (level != 0) : (level == 0);

    if (open == fsm->last_reported) {
        return SB_DOOR_EVT_NONE;
    }
    fsm->last_reported = open;
    return open ? SB_DOOR_EVT_OPEN : SB_DOOR_EVT_CLOSED;
}

size_t sb_door_build_event(char *buf, size_t buf_size, sb_door_event_t evt, uint32_t ts_ms)
{
    if (buf == NULL || buf_size == 0 || evt == SB_DOOR_EVT_NONE) {
        return 0;
    }
    const char *cmd = (evt == SB_DOOR_EVT_OPEN) ? "door_open" : "door_closed";
    int n = snprintf(buf, buf_size, "{\"type\":\"event\",\"cmd\":\"%s\",\"ts\":%lu}", cmd,
                     (unsigned long)ts_ms);
    return (n < 0 || n >= (int)buf_size) ? 0 : (size_t)n;
}

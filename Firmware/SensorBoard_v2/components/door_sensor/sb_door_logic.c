#include "sb_door_logic.h"

void sb_door_fsm_init(sb_door_fsm_t *fsm)
{
    (void)fsm;
}

sb_door_event_t sb_door_fsm_update(sb_door_fsm_t *fsm, int level, bool active_low)
{
    (void)fsm;
    (void)level;
    (void)active_low;
    return SB_DOOR_EVT_NONE;
}

size_t sb_door_build_event(char *buf, size_t buf_size, sb_door_event_t evt, uint32_t ts_ms)
{
    (void)buf;
    (void)buf_size;
    (void)evt;
    (void)ts_ms;
    return 0;
}

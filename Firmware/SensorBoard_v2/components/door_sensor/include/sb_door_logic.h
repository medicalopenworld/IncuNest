/* Parte pura de door_sensor (sin GPIO/RTOS): decisión de eventos y builder.
 * Expuesta en include/ para los tests Unity. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SB_DOOR_EVT_NONE = 0,
    SB_DOOR_EVT_OPEN,
    SB_DOOR_EVT_CLOSED,
} sb_door_event_t;

typedef struct {
    int last_reported; /* -1 = aún sin reportar (fuerza evento inicial) */
} sb_door_fsm_t;

void sb_door_fsm_init(sb_door_fsm_t *fsm);

/* Evalúa un nivel ESTABLE (post-debounce). Devuelve el evento a publicar,
 * o SB_DOOR_EVT_NONE si el estado no cambió desde el último reporte.
 * active_low: nivel 0 = imán presente = puerta cerrada. */
sb_door_event_t sb_door_fsm_update(sb_door_fsm_t *fsm, int level, bool active_low);

/* {"type":"event","cmd":"door_open"|"door_closed","ts":<ms>} — longitud o 0 */
size_t sb_door_build_event(char *buf, size_t buf_size, sb_door_event_t evt, uint32_t ts_ms);

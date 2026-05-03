/**
 * @file widget_temp_display.h
 * @brief Parameter tile widget: title, measured value (large), setpoint (small).
 */
#pragma once

#include "lvgl.h"
#include <stdbool.h>

typedef struct {
    const char *title;        /* e.g. "Temp. Aire" */
    const char *unit;         /* e.g. "°C" or "%" */
    lv_color_t  accent_color; /* tile accent strip colour */
} widget_temp_cfg_t;

lv_obj_t *widget_temp_display_create(lv_obj_t *parent, const widget_temp_cfg_t *cfg);

/* Update displayed values. Pass NULL to keep current text. */
void widget_temp_display_update(lv_obj_t *tile,
                                const char *measured,
                                const char *setpoint);

/* Set value colour (normal / warning / alarm) */
void widget_temp_display_set_state(lv_obj_t *tile, bool alarm);

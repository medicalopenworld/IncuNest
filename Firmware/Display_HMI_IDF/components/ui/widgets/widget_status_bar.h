/**
 * @file widget_status_bar.h
 * @brief Top status bar: product name, comm state, active alarm count.
 */
#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

lv_obj_t *widget_status_bar_create(lv_obj_t *parent);
void      widget_status_bar_set_comm(lv_obj_t *bar, bool connected);
void      widget_status_bar_set_alarms(lv_obj_t *bar, uint8_t count);

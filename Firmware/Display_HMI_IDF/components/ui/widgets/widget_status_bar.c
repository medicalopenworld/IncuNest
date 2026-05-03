/**
 * @file widget_status_bar.c
 * @brief Top status bar widget — 800×UI_STATUS_BAR_H dark strip.
 *
 * Layout (left → right):
 *   [IncuNest]   [● COMM / SIN COMM]   [bell N]   [vX.Y.Z]
 *
 * Internal handles stored in lv_obj user_data (heap struct via lv_mem_alloc).
 */

#include "widget_status_bar.h"
#include "ui_theme.h"
#include "version.h"

#include <stdio.h>

typedef struct {
    lv_obj_t *comm_dot;
    lv_obj_t *comm_label;
    lv_obj_t *alarm_label;
} status_bar_data_t;

/* ------------------------------------------------------------------ */

lv_obj_t *widget_status_bar_create(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), UI_STATUS_BAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_BAR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Product name — left */
    lv_obj_t *name = lv_label_create(bar);
    lv_label_set_text(name, "IncuNest");
    lv_obj_set_style_text_font(name, UI_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(name, UI_COLOR_TEXT_PRI, LV_PART_MAIN);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 12, 0);

    /* Firmware version — right */
    lv_obj_t *ver = lv_label_create(bar);
    lv_label_set_text(ver, "v" FW_VERSION_STR);
    lv_obj_set_style_text_font(ver, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(ver, UI_COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_align(ver, LV_ALIGN_RIGHT_MID, -12, 0);

    /* Alarm indicator — second from right */
    lv_obj_t *alarm_lbl = lv_label_create(bar);
    lv_label_set_text(alarm_lbl, LV_SYMBOL_BELL " 0");
    lv_obj_set_style_text_font(alarm_lbl, UI_FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(alarm_lbl, UI_COLOR_TEXT_SEC, LV_PART_MAIN);
    lv_obj_align(alarm_lbl, LV_ALIGN_RIGHT_MID, -80, 0);

    /* COMM dot — center */
    lv_obj_t *dot = lv_obj_create(bar);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dot, UI_COLOR_ALARM, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
    lv_obj_align(dot, LV_ALIGN_CENTER, -36, 0);

    lv_obj_t *comm_lbl = lv_label_create(bar);
    lv_label_set_text(comm_lbl, "SIN COMM");
    lv_obj_set_style_text_font(comm_lbl, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(comm_lbl, UI_COLOR_TEXT_SEC, LV_PART_MAIN);
    lv_obj_align(comm_lbl, LV_ALIGN_CENTER, 6, 0);

    /* Separator line at bottom */
    lv_obj_t *sep = lv_obj_create(bar);
    lv_obj_set_size(sep, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(sep, UI_COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);
    lv_obj_align(sep, LV_ALIGN_BOTTOM_MID, 0, 0);

    status_bar_data_t *d = lv_mem_alloc(sizeof(status_bar_data_t));
    d->comm_dot    = dot;
    d->comm_label  = comm_lbl;
    d->alarm_label = alarm_lbl;
    lv_obj_set_user_data(bar, d);

    return bar;
}

/* ------------------------------------------------------------------ */

void widget_status_bar_set_comm(lv_obj_t *bar, bool connected)
{
    status_bar_data_t *d = lv_obj_get_user_data(bar);
    if (!d) return;

    lv_obj_set_style_bg_color(d->comm_dot,
                              connected ? UI_COLOR_OK : UI_COLOR_ALARM, LV_PART_MAIN);
    lv_label_set_text(d->comm_label, connected ? "COMM OK" : "SIN COMM");
    lv_obj_set_style_text_color(d->comm_label,
                                connected ? UI_COLOR_OK : UI_COLOR_TEXT_SEC, LV_PART_MAIN);
}

/* ------------------------------------------------------------------ */

void widget_status_bar_set_alarms(lv_obj_t *bar, uint8_t count)
{
    status_bar_data_t *d = lv_obj_get_user_data(bar);
    if (!d) return;

    char buf[16];
    snprintf(buf, sizeof(buf), LV_SYMBOL_BELL " %u", (unsigned)count);
    lv_label_set_text(d->alarm_label, buf);
    lv_obj_set_style_text_color(d->alarm_label,
                                (count > 0) ? UI_COLOR_ALARM : UI_COLOR_TEXT_SEC, LV_PART_MAIN);
}

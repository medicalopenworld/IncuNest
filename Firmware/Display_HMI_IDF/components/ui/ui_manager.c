/**
 * @file ui_manager.c
 * @brief Screen navigation manager.
 *
 * Navigation bounce prevention — three independent layers:
 *
 *  Layer 1 — Same-screen guard:  if already on the target screen, return.
 *
 *  Layer 2 — Time guard (NAV_GUARD_MS): ignores navigate() calls arriving
 *             within 1000 ms of the previous navigation.
 *
 *  Layer 3 — LVGL object disable: immediately after loading a new screen,
 *             all its interactive buttons are put in LV_STATE_DISABLED so
 *             lv_obj_hit_test() returns false for them.  An LVGL timer
 *             re-enables them after NAV_LOCK_MS milliseconds.
 *             This is the most reliable layer because it operates at the
 *             LVGL object level — no touch-hardware timing dependencies.
 */

#include "ui_manager.h"
#include "screen_home.h"
#include "screen_settings.h"
#include "screen_alarms.h"
#include "lvgl_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "UI_MGR";

/* ------------------------------------------------------------------ */
#define NAV_GUARD_MS  1000u   /* minimum ms between navigations           */
#define NAV_LOCK_MS   1000u   /* ms to keep new-screen buttons disabled (= NAV_GUARD_MS) */

static screen_id_t s_current     = SCREEN_HOME;
static int64_t     s_last_nav_us = 0;

/* ------------------------------------------------------------------ */
/* LVGL timer callback — re-enables buttons on the screen that was
 * just loaded. user_data carries the screen_id_t as an integer. */

static void nav_unlock_cb(lv_timer_t *t)
{
    screen_id_t scr = (screen_id_t)(uintptr_t)t->user_data;
    switch (scr) {
        case SCREEN_ALARMS:   screen_alarms_set_nav_enabled(true);   break;
        case SCREEN_SETTINGS: screen_settings_set_nav_enabled(true); break;
        default: break;
    }
    lv_timer_del(t);
    ESP_LOGI(TAG, "Screen %d buttons unlocked", (int)scr);
}

/* ------------------------------------------------------------------ */

esp_err_t ui_manager_init(void)
{
    lvgl_port_lock(-1);
    screen_home_show();
    lvgl_port_unlock();

    /* Mark guard as already expired so the first user tap is never blocked */
    s_last_nav_us = esp_timer_get_time() - (int64_t)NAV_GUARD_MS * 1000LL;
    ESP_LOGI(TAG, "Home screen loaded");
    return ESP_OK;
}

/* ------------------------------------------------------------------ */

void ui_manager_navigate(screen_id_t screen)
{
    /* Layer 1 — same screen */
    if (screen == s_current) return;

    /* Layer 2 — time guard */
    int64_t now_us  = esp_timer_get_time();
    int64_t diff_ms = (now_us - s_last_nav_us) / 1000LL;
    if (diff_ms < (int64_t)NAV_GUARD_MS) {
        ESP_LOGI(TAG, "Nav to %d BLOCKED by time guard (%lld ms < %u ms)",
                 (int)screen, diff_ms, NAV_GUARD_MS);
        return;
    }

    s_last_nav_us = now_us;
    ESP_LOGI(TAG, "Navigate %d -> %d", (int)s_current, (int)screen);

    /* Load new screen — lock is already held (called from LVGL callback) */
    lvgl_port_lock(-1);
    switch (screen) {
        case SCREEN_HOME:     screen_home_show();     break;
        case SCREEN_SETTINGS: screen_settings_show(); break;
        case SCREEN_ALARMS:   screen_alarms_show();   break;
        default: break;
    }

    /* Suppress GT911 touch for NAV_LOCK_MS to discard stale finger-lift data */
    lvgl_port_suppress_touch_for_ms(NAV_LOCK_MS);

    /* Layer 3 — disable buttons on the new screen immediately, before any
     * bounce touch can reach them. An LVGL timer re-enables after NAV_LOCK_MS. */
    switch (screen) {
        case SCREEN_SETTINGS:
            screen_settings_set_nav_enabled(false);
            lv_timer_create(nav_unlock_cb, NAV_LOCK_MS,
                            (void *)(uintptr_t)SCREEN_SETTINGS);
            break;
        case SCREEN_ALARMS:
            screen_alarms_set_nav_enabled(false);
            lv_timer_create(nav_unlock_cb, NAV_LOCK_MS,
                            (void *)(uintptr_t)SCREEN_ALARMS);
            break;
        default:
            break;
    }

    lvgl_port_unlock();

    s_current = screen;
    ESP_LOGI(TAG, "Screen %d active. Buttons locked for %u ms.", (int)screen, NAV_LOCK_MS);
}

/* ------------------------------------------------------------------ */

void ui_manager_update_air_temp(const char *measured, const char *setpoint)
{
    if (s_current != SCREEN_HOME) return;
    lvgl_port_lock(-1);
    screen_home_update_air_temp(measured, setpoint);
    lvgl_port_unlock();
}

void ui_manager_update_humidity(const char *measured, const char *setpoint)
{
    if (s_current != SCREEN_HOME) return;
    lvgl_port_lock(-1);
    screen_home_update_humidity(measured, setpoint);
    lvgl_port_unlock();
}

void ui_manager_update_skin_temp(const char *measured, const char *setpoint)
{
    if (s_current != SCREEN_HOME) return;
    lvgl_port_lock(-1);
    screen_home_update_skin_temp(measured, setpoint);
    lvgl_port_unlock();
}

void ui_manager_set_comm_state(bool connected)
{
    if (s_current != SCREEN_HOME) return;
    lvgl_port_lock(-1);
    screen_home_set_comm_state(connected);
    lvgl_port_unlock();
}

void ui_manager_set_alarm_count(uint8_t count)
{
    if (s_current != SCREEN_HOME) return;
    lvgl_port_lock(-1);
    screen_home_set_alarm_count(count);
    lvgl_port_unlock();
}

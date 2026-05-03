/**
 * @file screen_home.h
 * @brief Home dashboard screen API.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void screen_home_show(void);

/* Update parameter tiles while home is the active screen */
void screen_home_update_air_temp(const char *measured, const char *setpoint);
void screen_home_update_humidity(const char *measured, const char *setpoint);
void screen_home_update_skin_temp(const char *measured, const char *setpoint);

/* Update status bar indicators */
void screen_home_set_comm_state(bool connected);
void screen_home_set_alarm_count(uint8_t count);

#ifdef __cplusplus
}
#endif

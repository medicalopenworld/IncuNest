/**
 * @file screen_alarms.h
 * @brief Alarms screen — stub navigable in FASE 2, full impl in FASE 4.
 */
#pragma once

#include <stdbool.h>

void screen_alarms_show(void);

/** Enable/disable the back button. Call with false immediately after show()
 *  to lock out touch bounce, then with true after the debounce window. */
void screen_alarms_set_nav_enabled(bool enabled);

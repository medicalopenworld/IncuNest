/**
 * @file screen_settings.h
 * @brief Settings screen — stub navigable in FASE 2, full impl in FASE 3.
 */
#pragma once

#include <stdbool.h>

void screen_settings_show(void);

/** Enable/disable the back button. Call with false immediately after show()
 *  to lock out touch bounce, then with true after the debounce window. */
void screen_settings_set_nav_enabled(bool enabled);

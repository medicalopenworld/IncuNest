#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "main.h"

void CreateUITask();

// Cambiar freq_write del bus RGB en runtime (reinicia el display)
void lcd_set_freq_write(uint32_t freq_hz);
uint32_t lcd_get_freq_write();

// UI Update functions (Exported for CommTask)
void update_labels();
void update_alarm_panels();
void AlarmSound_Update();
void chart_add_air_temp(float v);
void chart_add_skin_temp(float v);
void chart_add_hum_value(float hum);
void chart_save_history();
void update_history_charts();
void HistoryDropdown_cb(lv_event_t *e);
void ScreenCharts_load_cb(lv_event_t *e);
void ui_set_switch_state_silent(lv_obj_t *sw, bool on);
void UI_ApplyLanguage(ui_lang_t lang);
void UI_SyncAll();
void UI_ApplyTheme();
const char* getConnectivityString(int status, ui_lang_t lang);

// Globals exported for ElementsCreation.cpp
extern ui_lang_t g_lang;

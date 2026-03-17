#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "main.h"

void CreateUITask();

// UI Update functions (Exported for CommTask)
void update_labels();
void update_alarm_panels();
void AlarmSound_Update();
void chart_add_air_temp(float v);
void chart_add_skin_temp(float v);
void chart_add_hum_value(float hum);
void ui_set_switch_state_silent(lv_obj_t *sw, bool on);
void UI_ApplyLanguage(ui_lang_t lang);
void UI_SyncAll();

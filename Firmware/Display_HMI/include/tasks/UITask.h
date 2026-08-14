#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "main.h"

void CreateUITask();

extern SemaphoreHandle_t g_lvgl_mutex;
void LVGL_Mutex_Init(void);

static inline void LVGL_Lock(void) {
  if (g_lvgl_mutex) xSemaphoreTakeRecursive(g_lvgl_mutex, portMAX_DELAY);
}
static inline void LVGL_Unlock(void) {
  if (g_lvgl_mutex) xSemaphoreGiveRecursive(g_lvgl_mutex);
}

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
void temp_content_set_visible(bool visible);
void UI_ShowToast(const char *msg, uint32_t ms);
void ActivateTempControlUI(bool isAirMode);
// Resumes the phototherapy switch flow once the baby-data wizard finishes.
void ActivatePhototherapyFromWizard();
// Same, for the humidity switch flow.
void ActivateHumidityFromWizard();
void computeAndSendActuation(void);
bool UI_IsCriticalAlarmActive(void);
// True while any therapy is running (temperature, humidity or phototherapy),
// i.e. while a baby is under care. Gates both the baby-exit dialog and
// BabyWizard's already-identified-baby shortcut.
bool UI_AnyControlActive(void);
void UI_UpdatePowerBars(int tempPwm, int humPwm);
void UI_ApplyLanguage(ui_lang_t lang);
void UI_SyncAll();
void UI_ApplyTheme();
const char* getConnectivityString(int status, ui_lang_t lang);

// Globals exported for ElementsCreation.cpp
extern ui_lang_t g_lang;
extern lv_chart_series_t *lockPPGSeries;

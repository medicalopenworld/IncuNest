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

// true cuando UI_Task ya ha reservado los bounce buffers del panel RGB.
// setup() lo espera antes de arrancar WiFi (ver main.cpp).
bool UI_IsLcdPanelReady();

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
bool UI_IsAnyAlarmActive(void);
// Reafirma el banner de alarma y el icono AUDIO PAUSED en primer plano por
// encima de un overlay que se acaba de mover al frente (p.ej. FactoryTest).
// No-op si ninguno de los dos esta visible en ese momento.
void UI_ReassertAlarmOverlays(void);
// True while any therapy is running (temperature, humidity or phototherapy),
// i.e. while a baby is under care. Gates both the baby-exit dialog and
// BabyWizard's already-identified-baby shortcut.
bool UI_AnyControlActive(void);
// Evidencia del arranque para el test de fabrica HMI_I2C
// (shared-factory-test-bench2 D4): true si el init del touch (con
// reintentos) / de la secuencia de backlight (5 reintentos) tuvo exito. Un
// endTransmission() vacio no prueba nada frente al STC8H1K28 ni al GT911,
// que pueden NACKear una escritura vacia con hardware sano.
bool UI_TouchInitOk(void);
bool UI_BacklightInitOk(void);
void UI_UpdatePowerBars(int tempPwm, int humPwm);
void UI_ApplyLanguage(ui_lang_t lang);
void UI_SyncAll();
void UI_ApplyTheme();
const char* getConnectivityString(int status, ui_lang_t lang);

// --- Instantanea del estado de control de la UI (hmi-training-courses) ----
// Lo que una leccion interactiva puede cambiar en pantalla y hay que dejar
// como estaba al salir. Solo estado local de UITask: hmi_msg lo guarda y
// restaura training_mode.cpp por su cuenta.
struct UiControlSnapshot {
  double airTempValue, skinTempValue;
  int humValue;
  int selectedPanel, lastSelectedPanel;
  bool switchTemp, switchHum, tempSwitched, humSwitched, arrowsActive;
  bool skinPanelEnabled, darkMode, humidityEnabled;
  int photoTimerMinutes;
  bool photoTimerActive;
  unsigned long photoTimerStartMs;
  ui_lang_t lang;
  // Estado LV_STATE_CHECKED de los switches ocultos que gobiernan UI_SyncAll.
  bool sw1, sw2, sw3, sw4, swDark, swHum;
};
void UI_GetControlSnapshot(UiControlSnapshot *out);
// Restaura la instantanea sin disparar callbacks (ui_set_switch_state_silent)
// y reconstruye la UI con UI_SyncAll(). Bajo LVGL_Lock().
void UI_RestoreControlSnapshot(const UiControlSnapshot *s);
// Vuelve a poner el banner de alarma y el icono de AUDIO PAUSED en primer
// plano de lv_layer_top(). Llamar tras subir cualquier otro overlay de esa
// capa (el banner solo se reafirma solo cuando cambia su texto).
void UI_RaiseAlarmIndicators(void);

// Globals exported for ElementsCreation.cpp
extern lv_chart_series_t *lockPPGSeries;

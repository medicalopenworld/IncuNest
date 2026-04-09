#include "UITask.h"
#include "AudioManager.h"
#include "CommTask.h"
#include "buzzer.h"
#include "display_config.h"
#include "esp_log.h"
#include "main.h"
#include "ui.h"
#include <LovyanGFX.hpp>
#include <PCA9557.h>
#include <SPI.h>
#include <TAMC_GT911.h>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

static const char *TAG = "UI";

// --- Skin probe UI (RF-SKIN-008/009/010, UI-SKIN-002/003/004/005) ---
static lv_obj_t *ui_SkinProbeToast =
    nullptr; // Toast de bloqueo cuando sonda no válida
static lv_obj_t *ui_SkinProbeStatusLabel =
    nullptr; // Estado informativo en modo aire

static lv_obj_t *ui_AudioPlayBtn;
static lv_obj_t *ui_AudioStopBtn;
static lv_obj_t *ui_AudioPlayLabel;
static lv_obj_t *ui_VolumeLabel; // Muestra "Vol: XX"
static lv_obj_t *ui_VolumeUpBtn;
static lv_obj_t *ui_VolumeDownBtn;

// Novedades para Gráficas Históricas (Declaradas en ElementsCreation.cpp)
extern lv_obj_t *ui_HistoryChartAire;
extern lv_obj_t *ui_HistoryChartSkin;
extern lv_obj_t *ui_HistoryChartHum;
extern lv_obj_t *ui_HistoryValueAire;
extern lv_obj_t *ui_HistoryValueSkin;
extern lv_obj_t *ui_HistoryValueHum;

// Series de datos (Se definen aquí para ser usadas por UITask)
lv_chart_series_t *historySeriesAire = NULL;
lv_chart_series_t *historySeriesSkin = NULL;
lv_chart_series_t *historySeriesHum = NULL;

#define HISTORY_BUFFER_SIZE 720 // 2 horas a 10s/punto
float historyBufferAir[HISTORY_BUFFER_SIZE];
float historyBufferSkin[HISTORY_BUFFER_SIZE];
float historyBufferHum[HISTORY_BUFFER_SIZE];
int historyWriteIdx = 0;
int historySampleCount = 0;
static int decimationCounter = 0;

// ==========================================
// Globals
// ==========================================
double airTempValue, skinTempValue;
double airTempValueDetected = 0.00, skinTempValueDetected = 0.00;
int humValue;
int humValueDetected = 0;
int selectedPanel = NO_PANEL_SELECTED;
int lastSelectedPanel = NO_PANEL_SELECTED;
bool skinPanelEnabled = false;
bool switchTemp = false;
bool switchHum = false;
bool tempSwitched = false;
bool humSwitched = false;
bool arrowsActive = false;
bool darkMode = false;
int g_selectedAlarmId = -1;
volatile bool g_pendingAlarmUpdate = false;

bool alarmActive = false;
bool alarmsMuted = false;

bool prevTempAlarm = false;
bool prevHumAlarm = false;
int alarmSlotToIndex[MAX_ALARM_DISPLAY] = {-1, -1, -1, -1};

int chartLastPressed = -1;

bool wifiVisible = false;
bool isConnected = false;
static bool pendingReconnect = false;
static uint32_t disconnectTimestampMs = 0;
char wifi_ssid[64] = "";
char wifi_pass[64] = "";

bool LanguagesVisible = false;
bool locked = true;

lv_chart_series_t *airTempSeries = NULL;
lv_chart_series_t *skinTempSeries = NULL;
lv_chart_series_t *humSeries = NULL;

bool g_stateSynced = false;
uint32_t g_lastStateReqMs = 0;
bool g_ui_initialized = false;

// AUTO AIR state (ARQ-AUTOAIR-001)
bool   g_autoAirActive    = false;
static int g_babyWeightGrams  = 0;
static int g_babyGestWeeks    = 0;
static int g_babyAgeHours     = 0;   // age stored in hours for sub-day precision
static int g_popupWeight      = 1500;
static int g_popupGest        = 32;
static int g_popupAgeHours    = 0;   // hours in popup (0–23 = hours mode, ≥24 = days mode)
static float aa_popup_setpoint = 0.0f;
static float aa_popup_lo       = 0.0f;
static float aa_popup_hi       = 0.0f;

static bool eepromDirty = false;
static unsigned long lastVarChangeTime = 0;

ui_lang_t g_lang = LANG_EN;

// Phototherapy Timer
int photoTimerMinutes = 30; // Default
bool photoTimerActive = false;
unsigned long photoTimerStartMs = 0;
static int lastPhotoMinutesSent = -1;

Alarm alarmList[MAX_ALARMS];

// ==========================================
// LGFX Setup
// ==========================================
class LGFX : public lgfx::LGFX_Device {
public:
  lgfx::Bus_RGB _bus_instance;
  lgfx::Panel_RGB _panel_instance;

  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;

      // Pines del bus RGB — referencia oficial Elecrow CrowPanel 7.0
      // Fuente centralizada: include/display_config.h

      // Blue (B0-B4): d0..d4
      cfg.pin_d0 = DISPLAY_PIN_B0;
      cfg.pin_d1 = DISPLAY_PIN_B1;
      cfg.pin_d2 = DISPLAY_PIN_B2;
      cfg.pin_d3 = DISPLAY_PIN_B3;
      cfg.pin_d4 = DISPLAY_PIN_B4;

      // Green (G0-G5): d5..d10
      cfg.pin_d5 = DISPLAY_PIN_G0;
      cfg.pin_d6 = DISPLAY_PIN_G1;
      cfg.pin_d7 = DISPLAY_PIN_G2;
      cfg.pin_d8 = DISPLAY_PIN_G3;
      cfg.pin_d9 = DISPLAY_PIN_G4;
      cfg.pin_d10 = DISPLAY_PIN_G5;

      // Red (R0-R4): d11..d15
      cfg.pin_d11 = DISPLAY_PIN_R0;
      cfg.pin_d12 = DISPLAY_PIN_R1;
      cfg.pin_d13 = DISPLAY_PIN_R2;
      cfg.pin_d14 = DISPLAY_PIN_R3;
      cfg.pin_d15 = DISPLAY_PIN_R4;

      // Señales de control
      cfg.pin_henable = DISPLAY_PIN_DE;
      cfg.pin_vsync = DISPLAY_PIN_VSYNC;
      cfg.pin_hsync = DISPLAY_PIN_HSYNC;
      cfg.pin_pclk = DISPLAY_PIN_PCLK;
      cfg.freq_write = DISPLAY_FREQ_WRITE;

      // Timings de sincronización
      // CORRECCIÓN: polarity=0 (activo en LOW). El valor anterior (1) causaba
      // parpadeo RGB en algunas unidades por diferencias de tolerancia de fab.
      cfg.hsync_polarity = DISPLAY_HSYNC_POLARITY;
      cfg.hsync_front_porch = DISPLAY_HSYNC_FRONT_PORCH;
      cfg.hsync_pulse_width = DISPLAY_HSYNC_PULSE_WIDTH;
      cfg.hsync_back_porch = DISPLAY_HSYNC_BACK_PORCH;

      cfg.vsync_polarity = DISPLAY_VSYNC_POLARITY;
      cfg.vsync_front_porch = DISPLAY_VSYNC_FRONT_PORCH;
      cfg.vsync_pulse_width = DISPLAY_VSYNC_PULSE_WIDTH;
      cfg.vsync_back_porch = DISPLAY_VSYNC_BACK_PORCH;

      cfg.pclk_active_neg = DISPLAY_PCLK_ACTIVE_NEG;
      cfg.de_idle_high = DISPLAY_DE_IDLE_HIGH;
      cfg.pclk_idle_high = DISPLAY_PCLK_IDLE_HIGH;

      _bus_instance.config(cfg);
    }
    {
      auto cfg = _panel_instance.config_detail();
      cfg.use_psram = 1;
      _panel_instance.config_detail(cfg);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width = DISPLAY_WIDTH;
      cfg.memory_height = DISPLAY_HEIGHT;
      cfg.panel_width = DISPLAY_WIDTH;
      cfg.panel_height = DISPLAY_HEIGHT;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      _panel_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);
  }
};

LGFX lcd;

// Touch GT911 — pines y resolución desde display_config.h
TAMC_GT911 ts =
    TAMC_GT911(DISPLAY_TOUCH_SDA, DISPLAY_TOUCH_SCL, DISPLAY_TOUCH_INT,
               DISPLAY_TOUCH_RST, DISPLAY_WIDTH, DISPLAY_HEIGHT);

static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf[DISPLAY_WIDTH * DISPLAY_HEIGHT / COLOR_DIVISOR];
static lv_disp_drv_t disp_drv;
static lv_timer_t *intro_timer = NULL;

// ==========================================
// Definitions
// ==========================================
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                   lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + AREA_PIXEL_OFFSET);
  uint32_t h = (area->y2 - area->y1 + AREA_PIXEL_OFFSET);
  lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t *)&color_p->full);
  lcd.waitDMA();
  lv_disp_flush_ready(disp);
}

static void intro_timer_cb(lv_timer_t *t) {
  (void)t;
  lv_scr_load(ui_ScreenMain);
  if (intro_timer) {
    lv_timer_del(intro_timer);
    intro_timer = NULL;
  }
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  ts.read();
  if (ts.isTouched) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = ts.points[0].x;
    data->point.y = ts.points[0].y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void update_labels() {
  if (!g_ui_initialized)
    return;

  static double l_airDesired = -1.0, l_skinDesired = -1.0;
  static int l_humDesired = -1;
  static double l_airDet = -1.0, l_skinDet = -1.0;
  static int l_humDet = -1;
  static int l_photoMins = -1;

  char buffer[BUFFER_SIZE];

  if (abs(airTempValue - l_airDesired) > 0.05) {
    l_airDesired = airTempValue;
    snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValue);
    lv_label_set_text(ui_TempAirDesired, buffer);
    lv_label_set_text(ui_TargetAirTempNumLabel, buffer);
  }

  if (abs(skinTempValue - l_skinDesired) > 0.05) {
    l_skinDesired = skinTempValue;
    snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValue);
    lv_label_set_text(ui_TempSkinDesired, buffer);
    lv_label_set_text(ui_TargetSkinTempNumLabel, buffer);
  }

  if (humValue != l_humDesired) {
    l_humDesired = humValue;
    snprintf(buffer, sizeof(buffer), "%d%%", humValue);
    lv_label_set_text(ui_HumDesired, buffer);
    lv_label_set_text(ui_Label24, buffer);
  }

  if (abs(airTempValueDetected - l_airDet) > 0.05) {
    l_airDet = airTempValueDetected;
    snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValueDetected);
    lv_label_set_text(ui_TempAirDetected, buffer);
    lv_label_set_text(ui_TempAirDetectedRight, buffer);
    lv_label_set_text(ui_Label18, buffer);
  }

  if (abs(skinTempValueDetected - l_skinDet) > 0.05) {
    l_skinDet = skinTempValueDetected;
    snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValueDetected);
    lv_label_set_text(ui_TempSkinDetected, buffer);
    lv_label_set_text(ui_TempSkinDetectedRight, buffer);
    lv_label_set_text(ui_Label14, buffer);
  }

  if (humValueDetected != l_humDet) {
    l_humDet = humValueDetected;
    snprintf(buffer, sizeof(buffer), "%d%%", humValueDetected);
    lv_label_set_text(ui_HumDetected, buffer);
    lv_label_set_text(ui_HumDetectedRight, buffer);
    lv_label_set_text(ui_Label20, buffer);
  }

  int airBar = (airTempValueDetected <= 20.0)
                   ? 0
                   : (airTempValueDetected >= 40.0
                          ? 20
                          : (int)round(airTempValueDetected - 20.0));
  int skinBar = (skinTempValueDetected <= 20.0)
                    ? 0
                    : (skinTempValueDetected >= 40.0
                           ? 20
                           : (int)round(skinTempValueDetected - 20.0));
  int humBar = constrain(humValueDetected, 0, 100);

  lv_bar_set_value(ui_AirTempBar, airBar, LV_ANIM_OFF);
  lv_bar_set_value(ui_SkinTempBar, skinBar, LV_ANIM_OFF);
  lv_bar_set_value(ui_HumBar, humBar, LV_ANIM_OFF);

  // Update photo timer label if not active
  if (!photoTimerActive) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", photoTimerMinutes / 60,
             photoTimerMinutes % 60);
    lv_label_set_text(ui_PhotoTimeValueLabel, buf);
  }

  // Update History Screen Values
  if (ui_HistoryValueAire) {
    snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValueDetected);
    lv_label_set_text(ui_HistoryValueAire, buffer);
  }
  if (ui_HistoryValueSkin) {
    snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValueDetected);
    lv_label_set_text(ui_HistoryValueSkin, buffer);
  }
  if (ui_HistoryValueHum) {
    snprintf(buffer, sizeof(buffer), "%d%%", humValueDetected);
    lv_label_set_text(ui_HistoryValueHum, buffer);
  }

  // Derive skin probe presence from detected temperature and update switch
  // visibility
  static bool lastProbePresent = true; // force update on first call
  bool probePresent = (skinTempValueDetected > 0.1);
  g_skinProbeState = probePresent ? SKIN_PROBE_VALID : SKIN_PROBE_NOT_CONNECTED;
  if (probePresent != lastProbePresent) {
    lastProbePresent = probePresent;
    if (probePresent) {
      if (ui_Switch4)
        lv_obj_clear_flag(ui_Switch4, LV_OBJ_FLAG_HIDDEN);
      const char *text = (g_lang == LANG_ES)   ? "MODO PIEL"
                         : (g_lang == LANG_FR) ? "MODE PEAU"
                                               : "SKIN MODE";
      if (ui_SkinOptionLabel)
        lv_label_set_text(ui_SkinOptionLabel, text);
    } else {
      if (ui_Switch4)
        lv_obj_add_flag(ui_Switch4, LV_OBJ_FLAG_HIDDEN);
      const char *text = (g_lang == LANG_ES)   ? "Sin sonda de piel"
                         : (g_lang == LANG_FR) ? "Sonde peau absente"
                                               : "No skin probe detected";
      if (ui_SkinOptionLabel)
        lv_label_set_text(ui_SkinOptionLabel, text);
      if (skinPanelEnabled) {
        skinPanelEnabled = false;
        hmi_msg.skinModeEnabled = false;
        hmi_msg.shouldSendData = true;
        if (ui_SkinPanelCont)
          lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
}

const char *getConnectivityString(int status, ui_lang_t lang) {
  switch (status) {
  case COMM_STATUS_NONE:
    if (lang == LANG_ES)
      return "DESCONECTADO";
    if (lang == LANG_FR)
      return "DECONNECTE";
    return "DISCONNECTED";
  case COMM_STATUS_GPRS_ONLY:
    if (lang == LANG_ES)
      return "2G (SIN SERVER)";
    if (lang == LANG_FR)
      return "2G (SANS SERVEUR)";
    return "2G (NO SERVER)";
  case COMM_STATUS_GPRS_SERVER:
    if (lang == LANG_ES)
      return "2G + SERVIDOR";
    if (lang == LANG_FR)
      return "2G + SERVEUR";
    return "2G + SERVER";
  case COMM_STATUS_WIFI_ONLY:
    if (lang == LANG_ES)
      return "WIFI (SIN SERVER)";
    if (lang == LANG_FR)
      return "WIFI (SANS SERVEUR)";
    return "WIFI (NO SERVER)";
  case COMM_STATUS_WIFI_SERVER:
    if (lang == LANG_ES)
      return "WIFI + SERVIDOR";
    if (lang == LANG_FR)
      return "WIFI + SERVEUR";
    return "WIFI + SERVER";
  default:
    return "-";
  }
}

void UI_ApplyLanguage(ui_lang_t lang) {
  g_lang = lang;
  EEPROM.write(EEPROM_LANGUAGE, g_lang);
  eepromDirty = true;
  lastVarChangeTime = millis();

  const char *TXT_CONTROLTEMP[] = {"TEMPERATURA", "TEMPERATURE", "TEMPERATURE"};
  const char *TXT_CONTROLHUM[] = {"HUMEDAD", "HUMIDITY", "HUMIDITY"};
  const char *TXT_PHOTO[] = {"FOTOTERAPIA", "PHOTOTHERAPY", "PHOTOTHERAPIE"};
  const char *TXT_AIR[] = {"AIRE", "AIR", "AIR"};
  const char *TXT_SKIN[] = {"PIEL", "SKIN", "PEAU"};
  const char *TXT_ON[] = {"ON", "ON", "ON"};
  const char *TXT_OFF[] = {"OFF", "OFF", "OFF"};
  const char *TXT_SETTINGS[] = {"AJUSTES", "SETTINGS", "PARAMETRES"};
  const char *TXT_LANG[] = {"IDIOMA", "LANGUAGE", "LANGUE"};
  const char *TXT_WIFI[] = {"WIFI", "WIFI", "WIFI"};
  const char *TXT_CONNECT[] = {"CONECTAR", "CONNECT", "CONNEXION"};
  const char *TXT_SSID[] = {"SSID", "SSID", "SSID"};
  const char *TXT_PASSWORD[] = {"CONTRASENA", "PASSWORD", "MOT DE PASSE"};
  const char *TXT_SKINMODE[] = {"MODO PIEL", "SKIN MODE", "MODE PEAU"};
  const char *TXT_INFO[] = {"INFO", "INFO", "INFO"};
  const char *TXT_HMI_VERSION[] = {
      "VERSION PANTALLA:", "DISPLAY VERSION:", "VERSION ECRAN:"};
  const char *TXT_MB_VERSION[] = {
      "VERSION PLACA:", "MOTHERBOARD VERSION:", "VERSION CARTE:"};
  const char *TXT_SN[] = {"S/N:", "S/N:", "S/N:"};
  const char *TXT_HEATER_ERROR_RESTART[] = {
      "Error calentador\nToque para mas informacion",
      "Heater error\nTouch for more information",
      "Erreur chauffage\nToucher pour plus d'infos"};
  const char *TXT_ALARMS[] = {"ALARMAS", "ALARMS", "ALARMES"};
  const char *TXT_VIEWDETAIL[] = {"VER DETALLES", "VIEW DETAILS",
                                  "VOIR DETAILS"};
  const char *TXT_HUMCHART[] = {"GRAFICO HUMEDAD", "HUMIDITY CHART",
                                "GRAPHIQUE HUMIDITE"};
  const char *TXT_AIRTEMPCHART[] = {"GRAFICO TEMPERATURA AIRE",
                                    "AIR TEMPERATURE CHART",
                                    "GRAPHIQUE TEMPERATURE AIR"};
  const char *TXT_SKINTEMPCHART[] = {"GRAFICO TEMPERATURA PIEL",
                                     "SKIN TEMPERATURE CHART",
                                     "GRAPHIQUE TEMPERATURE PEAU"};
  const char *TXT_TABTEMP[] = {"TEMPERATURA", "TEMPERATURE", "TEMPERATURE"};
  const char *TXT_TABHUM[] = {"HUMEDAD", "HUMIDITY", "HUMIDITE"};
  const char *TXT_AIRTEMP[] = {
      "TEMPERATURA AIRE:", "AIR TEMPERATURE:", "AIR TEMPERATURE:"};
  const char *TXT_BABYTEMP[] = {
      "TEMPERATURA BEBE:", "BABY TEMPERATURE:", "TEMPERATURE BEBE:"};
  const char *TXT_HUM[] = {"HUMEDAD:", "HUMIDITY:", "HUMIDITE:"};
  const char *TXT_TARGETTEMP[] = {
      "TEMP. OBJETIVO:", "TARGET TEMP:", "TEMP. OBJECTIF:"};
  const char *TXT_TARGETHUM[] = {
      "HUMEDAD OBJETIVO:", "TARGET HUMIDITY:", "HUMIDITE OBJECTIF:"};
  const char *TXT_STATUS[] = {"ESTADO:", "STATUS:", "ETAT:"};
  const char *TXT_UNLOCK[] = {"PRESIONA 2 SEG\nPARA DESBLOQUEAR",
                              "PRESS 2 SEC \nTO UNLOCK",
                              "APPUYEZ 2 SEG\nPOUR DEVERROUILLER"};
  const char *TXT_INCUNEST[] = {"INCUNEST", "INCUNEST", "INCUNEST"};
  const char *TXT_SET[] = {"AJUSTAR", "SET", "REGLER"};
  const char *TXT_WIFISSID[] = {"WIFISSID", "WIFISSID", "WIFISSID"};
  const char *TXT_WIFICONNECTEDTO[] = {"WIFI CONECTADO A", "WIFI CONNECTED TO",
                                       "WIFI CONNECTE A"};
  const char *TXT_ALARMSDESC[] = {"DESCRIPCION DE ALARMAS",
                                  "ALARMS DESCRIPTION",
                                  "DESCRIPTION DES ALARMES"};
  const char *TXT_OXICHART[] = {"GRAFICO OXIMETRIA", "OXIMETRY CHART",
                                "GRAPHIQUE OXIMETRIE"};
  const char *TXT_PULSEOXIMETRY[] = {"PULSIOXIMETRIA", "PULSE OXIMETRY",
                                     "PULSIOXYMETRIE"};
  const char *TXT_LANG_OPTIONS[] = {"ESPANOL\nINGLES\nFRANCES",
                                    "SPANISH\nENGLISH\nFRENCH",
                                    "ESPAGNOL\nANGLAIS\nFRANCAIS"};
  const char *TXT_CONNECTIVITY[] = {
      "CONECTIVIDAD:", "CONNECTIVITY:", "CONNECTIVITE:"};
  const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
  const char *TXT_DARKMODE[] = {"MODO OSCURO", "DARK MODE", "MODE SOMBRE"};
  const char *TXT_REALTIME[] = {"TIEMPO REAL", "REAL TIME", "TEMPS REEL"};
  const char *TXT_HISTORY[] = {"HISTORIAL", "HISTORY", "HISTORIQUE"};
  const char *TXT_HISTORY_OPTIONS[] = {"5 min\n30 min\n1 h\n2 h",
                                       "5 min\n30 min\n1 h\n2 h",
                                       "5 min\n30 min\n1 h\n2 h"};
  const char *TXT_RANGE[] = {"RANGO:", "RANGE:", "PLAGE:"};
  const char *TXT_HIST_AIR[] = {"HISTORIAL TEMP AIRE", "AIR TEMP HISTORY",
                                "HIST. TEMP AIR"};
  const char *TXT_HIST_SKIN[] = {"HISTORIAL TEMP PIEL", "SKIN TEMP HISTORY",
                                 "HIST. TEMP PEAU"};
  const char *TXT_HIST_HUM[] = {"HISTORIAL TEMP HUM", "HUMIDITY HISTORY",
                                "HIST. HUMIDITÉ"};

  lv_label_set_text(ui_Label2, TXT_CONTROLTEMP[lang]);
  lv_label_set_text(ui_HumidityLabel, TXT_CONTROLHUM[lang]);
  lv_label_set_text(ui_PhototherapyLabel, TXT_PHOTO[lang]);
  lv_label_set_text(ui_Label30, TXT_AIR[lang]);
  lv_label_set_text(ui_Label31, TXT_SKIN[lang]);
  lv_label_set_text(ui_Label9, TXT_ON[lang]);
  lv_label_set_text(ui_Label15, TXT_OFF[lang]);
  lv_label_set_text(ui_Label13, TXT_ON[lang]);
  lv_label_set_text(ui_Label16, TXT_OFF[lang]);
  lv_label_set_text(ui_Label10, TXT_ON[lang]);
  lv_label_set_text(ui_Label17, TXT_OFF[lang]);
  lv_label_set_text(ui_Label8, TXT_SETTINGS[lang]);
  lv_label_set_text(ui_LanguagesLabel, TXT_LANG[lang]);
  lv_label_set_text(ui_WifiLabel, TXT_WIFI[lang]);
  lv_label_set_text(ui_ConnectLabel, TXT_CONNECT[lang]);
  lv_label_set_text(ui_SSIDLabel, TXT_SSID[lang]);
  lv_label_set_text(ui_PassLabel, TXT_PASSWORD[lang]);
  lv_label_set_text(ui_SkinOptionLabel, TXT_SKINMODE[lang]);
  lv_label_set_text(ui_InfoLabel, TXT_INFO[lang]);
  lv_label_set_text(ui_DarkModeLabel, TXT_DARKMODE[lang]);
  lv_label_set_text(ui_HMIVerTitle, TXT_HMI_VERSION[lang]);
  lv_label_set_text(ui_MBVerTitle, TXT_MB_VERSION[lang]);
  lv_label_set_text(ui_SNTitle, TXT_SN[lang]);
  if (ui_ConnTitle)
    lv_label_set_text(ui_ConnTitle, TXT_CONNECTIVITY[lang]);

  if (ui_HeaterErrorTempLabel)
    lv_label_set_text(ui_HeaterErrorTempLabel, TXT_HEATER_ERROR_RESTART[lang]);
  if (ui_HeaterErrorHumLabel)
    lv_label_set_text(ui_HeaterErrorHumLabel, TXT_HEATER_ERROR_RESTART[lang]);

  lv_label_set_text(ui_Label6, TXT_SET[lang]);
  lv_label_set_text(ui_Label7, TXT_SET[lang]);
  lv_label_set_text(ui_WifiSSIDLabel, TXT_WIFISSID[lang]);
  lv_label_set_text(ui_WifiConnectedToLabel, TXT_WIFICONNECTEDTO[lang]);
  lv_label_set_text(ui_AlarmDetailLabel, TXT_ALARMSDESC[lang]);
  lv_label_set_text(ui_Label35, TXT_OXICHART[lang]);
  lv_label_set_text(ui_Label39, TXT_PULSEOXIMETRY[lang]);
  lv_dropdown_set_options(ui_LanguagesDropDown, TXT_LANG_OPTIONS[lang]);
  lv_dropdown_set_selected(ui_LanguagesDropDown, lang);

  {
    lv_obj_t *btnm = lv_tabview_get_tab_btns(ui_AlarmsTabview);
    if (btnm && lv_obj_check_type(btnm, &lv_btnmatrix_class)) {
      static const char *map_alarm[3];
      map_alarm[0] = TXT_ALARMS[lang];
      map_alarm[1] = TXT_VIEWDETAIL[lang];
      map_alarm[2] = "";
      lv_btnmatrix_set_map(btnm, map_alarm);
    }
  }

  lv_label_set_text(ui_Label36, TXT_HUMCHART[lang]);
  lv_label_set_text(ui_Label37, TXT_AIRTEMPCHART[lang]);
  lv_label_set_text(ui_Label38, TXT_SKINTEMPCHART[lang]);
  {
    lv_obj_t *btnm = lv_tabview_get_tab_btns(ui_TabView1);
    if (btnm && lv_obj_check_type(btnm, &lv_btnmatrix_class)) {
      static const char *map_chart[3];
      map_chart[0] = TXT_TABTEMP[lang];
      map_chart[1] = TXT_TABHUM[lang];
      map_chart[2] = "";
      lv_btnmatrix_set_map(btnm, map_chart);
    }
  }

  // Traducción del TabView Principal de Gráficas
  if (ui_TabViewMainCharts) {
    lv_obj_t *btnm = lv_tabview_get_tab_btns(ui_TabViewMainCharts);
    if (btnm && lv_obj_check_type(btnm, &lv_btnmatrix_class)) {
      static const char *map_main_chart[3];
      map_main_chart[0] = TXT_REALTIME[lang];
      map_main_chart[1] = TXT_HISTORY[lang];
      map_main_chart[2] = "";
      lv_btnmatrix_set_map(btnm, map_main_chart);
    }
  }
  if (ui_HistoryDropdown) {
    lv_dropdown_set_options(ui_HistoryDropdown, TXT_HISTORY_OPTIONS[lang]);
  }
  if (ui_HistoryTimeLabel)
    lv_label_set_text(ui_HistoryTimeLabel, TXT_RANGE[lang]);
  if (ui_HistoryChartAireLabel)
    lv_label_set_text(ui_HistoryChartAireLabel, TXT_AIR[lang]);
  if (ui_HistoryChartSkinLabel)
    lv_label_set_text(ui_HistoryChartSkinLabel, TXT_SKIN[lang]);
  if (ui_HistoryChartHumLabel)
    lv_label_set_text(ui_HistoryChartHumLabel, TXT_CONTROLHUM[lang]);

  lv_label_set_text(ui_Label11, TXT_AIRTEMP[lang]);
  lv_label_set_text(ui_Label12, TXT_BABYTEMP[lang]);
  lv_label_set_text(ui_Label19, TXT_HUM[lang]);
  lv_label_set_text(ui_TargetAirTempLabel, TXT_TARGETTEMP[lang]);
  lv_label_set_text(ui_TargetSkinTempLabel, TXT_TARGETTEMP[lang]);
  lv_label_set_text(ui_Label23, TXT_TARGETHUM[lang]);
  lv_label_set_text(ui_StatusLabel, TXT_STATUS[lang]);
  lv_label_set_text(ui_Label4, TXT_UNLOCK[lang]);

  // Phototherapy Timer
  if (ui_PhotoLockLabel) {
    const char *TXT_PHOTO_LOCK[] = {
        "FOTOTERAPIA:", "PHOTOTHERAPY:", "PHOTOTHERAPIE:"};
    lv_label_set_text(ui_PhotoLockLabel, TXT_PHOTO_LOCK[lang]);
  }

  update_labels();
  UI_SyncAll();
}

void LanguagesDropDown_cb(lv_event_t *e) {
  lv_obj_t *dd = lv_event_get_target(e);
  uint16_t sel = lv_dropdown_get_selected(dd);
  if (sel == 0)
    UI_ApplyLanguage(LANG_ES);
  else if (sel == 1)
    UI_ApplyLanguage(LANG_EN);
  else if (sel == 2)
    UI_ApplyLanguage(LANG_FR);

  hmi_msg.language = sel;
  hmi_msg.shouldSendData = true;
}

lv_chart_series_t *configure_temp_chart(lv_obj_t *chart, lv_palette_t pal,
                                        lv_coord_t min, lv_coord_t max) {
  lv_chart_series_t *s = lv_chart_get_series_next(chart, NULL);
  while (s != NULL) {
    lv_chart_series_t *next = lv_chart_get_series_next(chart, s);
    s->x_points = NULL;
    s->x_ext_buf_assigned = 0;
    lv_chart_remove_series(chart, s);
    s = next;
  }
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, 50);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, min, max);
  lv_chart_series_t *series =
      lv_chart_add_series(chart, lv_palette_main(pal), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 0, 0, 0, false,
                         0);
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0, false, 0);
  for (int i = 0; i < lv_chart_get_point_count(chart); i++) {
    series->y_points[i] = LV_CHART_POINT_NONE;
  }
  lv_chart_refresh(chart);
  return series;
}

void temp_chart_show_for_selected_panel(void) {
  if (!tempSwitched) {
    lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (selectedPanel != AIR_PANEL_SELECTED &&
      selectedPanel != SKIN_PANEL_SELECTED) {
    selectedPanel = AIR_PANEL_SELECTED;
    lastSelectedPanel = selectedPanel;
    hmi_msg.controlMode = CONTROL_AIR;
  }
  lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
  if (selectedPanel == AIR_PANEL_SELECTED) {
    lv_obj_clear_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
  }
}

void chart_add_air_temp(float v) {
  if (!airTempSeries)
    return;
  lv_chart_set_next_value(ui_AirTempChart, airTempSeries, (lv_coord_t)v);
}

void chart_add_skin_temp(float v) {
  if (!skinTempSeries)
    return;
  lv_chart_set_next_value(ui_SkinTempChart, skinTempSeries, (lv_coord_t)v);
}

// ============================================================================
// AUTO AIR — Neutral Thermal Environment table & UI logic
// ARQ-AUTOAIR-001..010 / UI-AUTOAIR-001..010
// Clinical source: "Termorregulacion y Humedad en el RN"
// ============================================================================

// Returns the midpoint setpoint (°C) from the Neutral Thermal Environment table.
// Also writes a short row description into row_desc for audit logging.
// Returns -1.0f on invalid input.
static float autoair_calculate_setpoint(int weightGrams, int gestWeeks,
                                          int ageHours,
                                          char *row_desc, size_t desc_len,
                                          float *lo_out = nullptr,
                                          float *hi_out = nullptr) {
  if (weightGrams <= 0 || gestWeeks <= 0 || ageHours < 0) {
    snprintf(row_desc, desc_len, "invalid inputs");
    return -1.0f;
  }
  // Conservative rule: >2500g with EG<=36wk → use 1501-2500g row
  bool heavy = (weightGrams > 2500) && (gestWeeks > 36);
  float lo, hi;

  if (ageHours < 96) {
    bool vLight  = (weightGrams < 1200);
    bool medLow  = (weightGrams >= 1200 && weightGrams <= 1500);
    bool medHigh = (!vLight && !medLow && !heavy);
    if (ageHours < 6) {
      if      (vLight)  { lo=34.0f; hi=35.4f; snprintf(row_desc,desc_len,"0-6h,<1200g"); }
      else if (medLow)  { lo=33.9f; hi=34.4f; snprintf(row_desc,desc_len,"0-6h,1200-1500g"); }
      else if (medHigh) { lo=32.8f; hi=33.8f; snprintf(row_desc,desc_len,"0-6h,1501-2500g"); }
      else              { lo=32.0f; hi=33.8f; snprintf(row_desc,desc_len,"0-6h,>2500g+>36sem"); }
    } else if (ageHours < 12) {
      if      (vLight)  { lo=34.0f; hi=35.4f; snprintf(row_desc,desc_len,"6-12h,<1200g"); }
      else if (medLow)  { lo=33.5f; hi=34.4f; snprintf(row_desc,desc_len,"6-12h,1200-1500g"); }
      else if (medHigh) { lo=32.2f; hi=33.8f; snprintf(row_desc,desc_len,"6-12h,1501-2500g"); }
      else              { lo=31.4f; hi=33.8f; snprintf(row_desc,desc_len,"6-12h,>2500g+>36sem"); }
    } else if (ageHours < 24) {
      if      (vLight)  { lo=34.0f; hi=35.4f; snprintf(row_desc,desc_len,"12-24h,<1200g"); }
      else if (medLow)  { lo=33.3f; hi=34.3f; snprintf(row_desc,desc_len,"12-24h,1200-1500g"); }
      else if (medHigh) { lo=31.8f; hi=33.8f; snprintf(row_desc,desc_len,"12-24h,1501-2500g"); }
      else              { lo=31.0f; hi=33.7f; snprintf(row_desc,desc_len,"12-24h,>2500g+>36sem"); }
    } else if (ageHours < 36) {
      if      (vLight)  { lo=34.0f; hi=35.0f; snprintf(row_desc,desc_len,"24-36h,<1200g"); }
      else if (medLow)  { lo=33.1f; hi=34.2f; snprintf(row_desc,desc_len,"24-36h,1200-1500g"); }
      else if (medHigh) { lo=31.6f; hi=33.6f; snprintf(row_desc,desc_len,"24-36h,1501-2500g"); }
      else              { lo=30.7f; hi=33.5f; snprintf(row_desc,desc_len,"24-36h,>2500g+>36sem"); }
    } else if (ageHours < 48) {
      if      (vLight)  { lo=34.0f; hi=35.0f; snprintf(row_desc,desc_len,"36-48h,<1200g"); }
      else if (medLow)  { lo=33.0f; hi=34.1f; snprintf(row_desc,desc_len,"36-48h,1200-1500g"); }
      else if (medHigh) { lo=31.4f; hi=33.5f; snprintf(row_desc,desc_len,"36-48h,1501-2500g"); }
      else              { lo=30.5f; hi=33.3f; snprintf(row_desc,desc_len,"36-48h,>2500g+>36sem"); }
    } else if (ageHours < 72) {
      if      (vLight)  { lo=34.0f; hi=35.0f; snprintf(row_desc,desc_len,"48-72h,<1200g"); }
      else if (medLow)  { lo=33.0f; hi=34.0f; snprintf(row_desc,desc_len,"48-72h,1200-1500g"); }
      else if (medHigh) { lo=31.2f; hi=33.4f; snprintf(row_desc,desc_len,"48-72h,1501-2500g"); }
      else              { lo=30.1f; hi=33.2f; snprintf(row_desc,desc_len,"48-72h,>2500g+>36sem"); }
    } else {                                                    // 72-96 h
      if      (vLight)  { lo=34.0f; hi=35.0f; snprintf(row_desc,desc_len,"72-96h,<1200g"); }
      else if (medLow)  { lo=33.0f; hi=34.0f; snprintf(row_desc,desc_len,"72-96h,1200-1500g"); }
      else if (medHigh) { lo=31.1f; hi=33.2f; snprintf(row_desc,desc_len,"72-96h,1501-2500g"); }
      else              { lo=29.8f; hi=32.8f; snprintf(row_desc,desc_len,"72-96h,>2500g+>36sem"); }
    }
  } else if (ageHours < 288) {                                  // 4-12 days
    bool lt1500 = (weightGrams < 1500);
    bool m15_25 = (!lt1500 && !heavy);
    if (lt1500) {
      lo=33.0f; hi=34.0f; snprintf(row_desc,desc_len,"4-12d,<1500g");
    } else if (m15_25) {
      lo=31.0f; hi=33.2f; snprintf(row_desc,desc_len,"4-12d,1500-2500g");
    } else {
      if      (ageHours < 120) { lo=29.5f; hi=32.6f; snprintf(row_desc,desc_len,"4-5d,>2500g+>36sem"); }
      else if (ageHours < 144) { lo=29.4f; hi=32.3f; snprintf(row_desc,desc_len,"5-6d,>2500g+>36sem"); }
      else if (ageHours < 192) { lo=29.0f; hi=32.2f; snprintf(row_desc,desc_len,"6-8d,>2500g+>36sem"); }
      else if (ageHours < 240) { lo=29.0f; hi=32.0f; snprintf(row_desc,desc_len,"8-10d,>2500g+>36sem"); }
      else                     { lo=29.0f; hi=31.4f; snprintf(row_desc,desc_len,"10-12d,>2500g+>36sem"); }
    }
  } else if (ageHours < 336) {                                  // 12-14 days
    bool lt1500 = (weightGrams < 1500);
    bool m15_25 = (!lt1500 && !heavy);
    if      (lt1500)  { lo=32.6f; hi=34.0f; snprintf(row_desc,desc_len,"12-14d,<1500g"); }
    else if (m15_25)  { lo=31.0f; hi=33.2f; snprintf(row_desc,desc_len,"12-14d,1500-2500g"); }
    else              { lo=29.0f; hi=30.8f; snprintf(row_desc,desc_len,"12-14d,>2500g+>36sem"); }
  } else if (ageHours < 504) {                                  // 2-3 weeks
    bool lt1500 = (weightGrams < 1500);
    if (lt1500) { lo=32.2f; hi=34.0f; snprintf(row_desc,desc_len,"2-3sem,<1500g"); }
    else        { lo=30.5f; hi=33.0f; snprintf(row_desc,desc_len,"2-3sem,1500-2500g"); }
  } else if (ageHours < 672) {                                  // 3-4 weeks
    bool lt1500 = (weightGrams < 1500);
    if (lt1500) { lo=31.6f; hi=33.6f; snprintf(row_desc,desc_len,"3-4sem,<1500g"); }
    else        { lo=30.0f; hi=32.7f; snprintf(row_desc,desc_len,"3-4sem,1500-2500g"); }
  } else {                                                      // >4 weeks (approximation)
    bool lt1500 = (weightGrams < 1500);
    if (lt1500) { lo=31.6f; hi=33.6f; snprintf(row_desc,desc_len,">4sem,<1500g(aprox)"); }
    else        { lo=30.0f; hi=32.7f; snprintf(row_desc,desc_len,">4sem,>1500g(aprox)"); }
  }
  if (lo_out) *lo_out = lo;
  if (hi_out) *hi_out = hi;
  return (lo + hi) / 2.0f;
}

static void autoair_show_toast(const char *msg, uint32_t ms) {
  if (!ui_AutoAirToast) return;
  lv_label_set_text(ui_AutoAirToast, msg);
  lv_obj_clear_flag(ui_AutoAirToast, LV_OBJ_FLAG_HIDDEN);
  lv_timer_create(
      [](lv_timer_t *t) {
        if (ui_AutoAirToast)
          lv_obj_add_flag(ui_AutoAirToast, LV_OBJ_FLAG_HIDDEN);
        lv_timer_del(t);
      },
      ms, nullptr);
}

static void autoair_update_button_style() {
  if (!ui_AutoAirBtn || !ui_AutoAirBtnLabel) return;
  bool inAirMode = (selectedPanel == AIR_PANEL_SELECTED) && tempSwitched;

  if (!inAirMode) {
    // Disabled — skin mode or no temperature switch active
    lv_obj_clear_flag(ui_AutoAirBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_AutoAirBtn, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_AutoAirBtn, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_AutoAirBtnLabel, COLOR_PANEL_LIGHT_GRAY, 0);
  } else if (g_autoAirActive) {
    // Active — AUTO AIR running
    lv_obj_add_flag(ui_AutoAirBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_AutoAirBtn, lv_color_hex(0x0095DA), LV_PART_MAIN);
    lv_obj_set_style_opa(ui_AutoAirBtn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_AutoAirBtnLabel, lv_color_hex(0xFFFFFF), 0);
  } else {
    // Passive — AIR mode, waiting for activation
    lv_color_t bg = darkMode ? COLOR_PANEL_GRAY : COLOR_PANEL_WHITE;
    lv_color_t fg = darkMode ? COLOR_TEXT_DARK  : lv_color_make(30, 30, 30);
    lv_obj_add_flag(ui_AutoAirBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_AutoAirBtn, bg, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_AutoAirBtn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_AutoAirBtnLabel, fg, 0);
  }

  // Temperature arrows: disabled (grey, non-clickable) while AUTO AIR is active
  if (g_autoAirActive) {
    lv_obj_clear_flag(ui_ImgArrowUpTemp,   LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_img_recolor(ui_ImgArrowUpTemp,   lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(ui_ImgArrowUpTemp,   LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(ui_ImgArrowDownTemp, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(ui_ImgArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
  } else {
    lv_obj_add_flag(ui_ImgArrowUpTemp,   LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_img_recolor_opa(ui_ImgArrowUpTemp,   LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(ui_ImgArrowDownTemp, LV_OPA_TRANSP, LV_PART_MAIN);
  }
}

static void autoair_deactivate(bool fromModeSwitch) {
  if (!g_autoAirActive) return;
  g_autoAirActive = false;

  // Re-enable manual temperature arrows
  if (arrowsActive) {
    lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_color_t active_col = darkMode ? COLOR_PANEL_GRAY : COLOR_PANEL_WHITE;
    lv_obj_set_style_bg_color(ui_ArrowUpTemp, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowDownTemp, active_col, LV_PART_MAIN);
  }
  autoair_update_button_style();

  const char *msg;
  if (fromModeSwitch) {
    msg = (g_lang == LANG_ES)
              ? "AUTO AIR desactivado al cambiar a modo PIEL"
          : (g_lang == LANG_FR)
              ? "AUTO AIR desactive: passage en mode PEAU"
              : "AUTO AIR auto-deactivated: switched to SKIN mode";
  } else {
    msg = (g_lang == LANG_ES)
              ? "AUTO AIR desactivado - Control manual activo"
          : (g_lang == LANG_FR)
              ? "AUTO AIR desactive - Controle manuel actif"
              : "AUTO AIR deactivated - Manual control active";
  }
  autoair_show_toast(msg, 3500);
  ESP_LOGI(TAG,
           "[AUTO-AIR] Deactivated. fromModeSwitch=%d "
           "weight=%dg gest=%dwk age=%dh",
           (int)fromModeSwitch, g_babyWeightGrams,
           g_babyGestWeeks, g_babyAgeHours);
}

static void autoair_activate(float setpoint, const char *rowDesc) {
  // Clamp to AIR range and round to 0.2 °C step
  if (setpoint < (float)AIR_TEMP_MIN) setpoint = (float)AIR_TEMP_MIN;
  if (setpoint > (float)AIR_TEMP_MAX) setpoint = (float)AIR_TEMP_MAX;
  setpoint = (float)((int)(setpoint * 5.0f + 0.5f)) * 0.2f;

  g_autoAirActive   = true;
  airTempValue      = setpoint;
  hmi_msg.desiredAirTemperature = airTempValue;
  hmi_msg.shouldSendData = true;
  EEPROM.writeFloat(EEPROM_DESIRED_AIR_TEMP, airTempValue);
  eepromDirty = true;
  lastVarChangeTime = millis();

  // Disable manual arrows (AUTO AIR owns the setpoint)
  lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);

  autoair_update_button_style();
  update_labels();

  const char *msg = (g_lang == LANG_ES)
      ? "AUTO AIR activado - Ajuste automatico segun peso y EG"
  : (g_lang == LANG_FR)
      ? "AUTO AIR active - Reglage automatique poids/AG"
      : "AUTO AIR activated - Auto setpoint by weight & GA";
  autoair_show_toast(msg, 4000);

  ESP_LOGI(TAG,
           "[AUTO-AIR] Activated. Setpoint=%.1f degC row='%s' "
           "weight=%dg gest=%dwk age=%dh ts=%lu",
           setpoint, rowDesc,
           g_babyWeightGrams, g_babyGestWeeks, g_babyAgeHours,
           (unsigned long)millis());
}

static void aa_update_range_display() {
  if (!aa_range_bar || !aa_label_hi || !aa_label_mid || !aa_label_lo || !aa_setpoint_label) return;
  char rowDesc[48];
  float lo, hi;
  float sp = autoair_calculate_setpoint(g_popupWeight, g_popupGest, g_popupAgeHours,
                                         rowDesc, sizeof(rowDesc), &lo, &hi);
  char buf[16];
  if (sp < 0.0f) {
    lv_label_set_text(aa_label_hi,  "--.-");
    lv_label_set_text(aa_label_mid, "--.-");
    lv_label_set_text(aa_label_lo,  "--.-");
    lv_label_set_text(aa_setpoint_label, "--.-");
    lv_bar_set_start_value(aa_range_bar, 280, LV_ANIM_OFF);
    lv_bar_set_value(aa_range_bar, 280, LV_ANIM_OFF);
    aa_popup_lo = 0.0f; aa_popup_hi = 0.0f; aa_popup_setpoint = 0.0f;
    return;
  }
  aa_popup_lo = lo;
  aa_popup_hi = hi;
  // Midpoint rounded to 0.2°C grid
  float mid = (float)((int)(sp * 5.0f + 0.5f)) * 0.2f;
  if (mid < lo) mid = lo;
  if (mid > hi) mid = hi;
  aa_popup_setpoint = mid;

  // Bar: range 280–370 (°C × 10)
  lv_bar_set_start_value(aa_range_bar, (int)(lo * 10.0f), LV_ANIM_OFF);
  lv_bar_set_value(aa_range_bar, (int)(hi * 10.0f), LV_ANIM_OFF);

  snprintf(buf, sizeof(buf), "%.1f", hi);
  lv_label_set_text(aa_label_hi, buf);
  snprintf(buf, sizeof(buf), "%.1f", aa_popup_setpoint);
  lv_label_set_text(aa_label_mid, buf);
  snprintf(buf, sizeof(buf), "%.1f", lo);
  lv_label_set_text(aa_label_lo, buf);
  snprintf(buf, sizeof(buf), "%.1f C", aa_popup_setpoint);
  lv_label_set_text(aa_setpoint_label, buf);
}

static void autoair_popup_update_labels() {
  if (!ui_AutoAirWeightVal || !ui_AutoAirGestVal || !ui_AutoAirDaysVal) return;
  char buf[12];
  snprintf(buf, sizeof(buf), "%d", g_popupWeight);
  lv_label_set_text(ui_AutoAirWeightVal, buf);
  snprintf(buf, sizeof(buf), "%d", g_popupGest);
  lv_label_set_text(ui_AutoAirGestVal, buf);
  if (g_popupAgeHours < 24) {
    snprintf(buf, sizeof(buf), "%d", g_popupAgeHours);
    if (ui_AutoAirDaysUnitLbl) lv_label_set_text(ui_AutoAirDaysUnitLbl, "HRS");
  } else {
    snprintf(buf, sizeof(buf), "%d", g_popupAgeHours / 24);
    if (ui_AutoAirDaysUnitLbl) lv_label_set_text(ui_AutoAirDaysUnitLbl, "DAYS");
  }
  lv_label_set_text(ui_AutoAirDaysVal, buf);
  aa_update_range_display();
}

static void autoair_popup_show(bool show) {
  if (!ui_AutoAirOverlay) return;
  if (show) {
    g_popupWeight = (g_babyWeightGrams > 0)  ? g_babyWeightGrams : 1500;
    g_popupGest   = (g_babyGestWeeks   > 0)  ? g_babyGestWeeks   : 32;
    g_popupAgeHours = (g_babyAgeHours >= 0) ? g_babyAgeHours : 0;
    autoair_popup_update_labels();
    if (ui_AutoAirErrLabel)
      lv_obj_add_flag(ui_AutoAirErrLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_AutoAirOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_AutoAirOverlay);
  } else {
    lv_obj_add_flag(ui_AutoAirOverlay, LV_OBJ_FLAG_HIDDEN);
  }
}

// Popup spinbox callbacks
void aa_weight_dec_cb(lv_event_t *) {
  if (g_popupWeight > 100) g_popupWeight -= 50;
  autoair_popup_update_labels();
}
void aa_weight_inc_cb(lv_event_t *) {
  if (g_popupWeight < 5000) g_popupWeight += 50;
  autoair_popup_update_labels();
}
void aa_gest_dec_cb(lv_event_t *) {
  if (g_popupGest > 23) g_popupGest--;
  autoair_popup_update_labels();
}
void aa_gest_inc_cb(lv_event_t *) {
  if (g_popupGest < 44) g_popupGest++;
  autoair_popup_update_labels();
}
void aa_days_dec_cb(lv_event_t *) {
  if      (g_popupAgeHours > 24)  g_popupAgeHours -= 24; // bajar 1 día
  else if (g_popupAgeHours == 24) g_popupAgeHours = 23;  // cruzar a modo horas
  else if (g_popupAgeHours > 0)   g_popupAgeHours--;     // bajar 1 hora
  autoair_popup_update_labels();
}
void aa_days_inc_cb(lv_event_t *) {
  if      (g_popupAgeHours < 23)       g_popupAgeHours++;     // subir 1 hora
  else if (g_popupAgeHours == 23)      g_popupAgeHours = 24;  // cruzar a modo días
  else if (g_popupAgeHours < 28 * 24)  g_popupAgeHours += 24; // subir 1 día
  autoair_popup_update_labels();
}

void aa_confirm_cb(lv_event_t *) {
  if (g_popupWeight <= 0 || g_popupGest <= 0 || g_popupAgeHours < 0) {
    if (ui_AutoAirErrLabel) {
      const char *errTxt = (g_lang == LANG_ES)
          ? "Faltan datos del bebe"
      : (g_lang == LANG_FR)
          ? "Donnees du bebe manquantes"
          : "Missing baby data";
      lv_label_set_text(ui_AutoAirErrLabel, errTxt);
      lv_obj_clear_flag(ui_AutoAirErrLabel, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }
  if (aa_popup_setpoint <= 0.0f) {
    if (ui_AutoAirErrLabel) {
      const char *errTxt = (g_lang == LANG_ES)
          ? "Rango no calculado"
      : (g_lang == LANG_FR)
          ? "Plage non calculee"
          : "Range not computed";
      lv_label_set_text(ui_AutoAirErrLabel, errTxt);
      lv_obj_clear_flag(ui_AutoAirErrLabel, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }
  g_babyWeightGrams = g_popupWeight;
  g_babyGestWeeks   = g_popupGest;
  g_babyAgeHours    = g_popupAgeHours;
  char rowDesc[48];
  autoair_calculate_setpoint(g_babyWeightGrams, g_babyGestWeeks,
                              g_babyAgeHours, rowDesc, sizeof(rowDesc));
  autoair_popup_show(false);
  autoair_activate(aa_popup_setpoint, rowDesc);
}

void aa_cancel_cb(lv_event_t *) {
  autoair_popup_show(false);
}

void aa_setpoint_up_cb(lv_event_t *) {
  if (aa_popup_hi <= 0.0f) return;
  int steps = (int)(aa_popup_setpoint * 5.0f + 0.5f) + 1;
  float next = (float)steps * 0.2f;
  if (next > aa_popup_hi) next = aa_popup_hi;
  aa_popup_setpoint = next;
  if (aa_label_mid && aa_setpoint_label) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1f", aa_popup_setpoint);
    lv_label_set_text(aa_label_mid, buf);
    snprintf(buf, sizeof(buf), "%.1f C", aa_popup_setpoint);
    lv_label_set_text(aa_setpoint_label, buf);
  }
}

void aa_setpoint_down_cb(lv_event_t *) {
  if (aa_popup_lo <= 0.0f) return;
  int steps = (int)(aa_popup_setpoint * 5.0f + 0.5f) - 1;
  float next = (float)steps * 0.2f;
  if (next < aa_popup_lo) next = aa_popup_lo;
  aa_popup_setpoint = next;
  if (aa_label_mid && aa_setpoint_label) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1f", aa_popup_setpoint);
    lv_label_set_text(aa_label_mid, buf);
    snprintf(buf, sizeof(buf), "%.1f C", aa_popup_setpoint);
    lv_label_set_text(aa_setpoint_label, buf);
  }
}

void AutoAirBtn_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  if (selectedPanel != AIR_PANEL_SELECTED || !tempSwitched) {
    const char *msg = (g_lang == LANG_ES)
        ? "Disponible solo en modo AIR"
    : (g_lang == LANG_FR)
        ? "Disponible uniquement en mode AIR"
        : "Available in AIR mode only";
    autoair_show_toast(msg, 3000);
    return;
  }
  if (g_autoAirActive) {
    autoair_deactivate(false);
    return;
  }
  autoair_popup_show(true);
}

void set_active_panel(lv_obj_t *active, lv_obj_t *inactive) {
  lv_color_t active_col = darkMode ? COLOR_PANEL_GRAY : COLOR_PANEL_WHITE;
  lv_color_t inactive_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_GRAY;

  lv_obj_set_style_bg_color(active, active_col, LV_PART_MAIN);
  lv_obj_set_style_opa(active, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(inactive, inactive_col, LV_PART_MAIN);
  lv_obj_set_style_opa(inactive, LV_OPA_COVER, LV_PART_MAIN);
}

void updateButtonVisibility() {
  if (isConnected) {
    lv_obj_add_flag(ui_WifiConnectButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_WifiDisconnectButton, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(ui_WifiConnectButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_WifiDisconnectButton, LV_OBJ_FLAG_HIDDEN);
  }
}

void WifiButton_cb(lv_event_t *e) {
  lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  LanguagesVisible = false;
  lv_obj_add_flag(ui_InfoDetailsCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  isConnected = (WiFi.status() == WL_CONNECTED);
  if (isConnected) {
    lv_obj_clear_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  }
  updateButtonVisibility();
  wifiVisible = true;
  hmi_msg.shouldSendData = true;
}

void InfoButton_cb(lv_event_t *e) {
  lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  LanguagesVisible = false;
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);

  lv_obj_clear_flag(ui_InfoDetailsCont, LV_OBJ_FLAG_HIDDEN);

  // Update values
  lv_label_set_text(ui_HMIVerValue, FWversion);
  lv_label_set_text(ui_MBVerValue, ctrl_state_msg.fwVer);
  char snBuf[16];
  snprintf(snBuf, sizeof(snBuf), "%04d", in3.serialNumber);
  lv_label_set_text(ui_SNValue, snBuf);

  if (ui_ConnValue) {
    lv_label_set_text(
        ui_ConnValue,
        getConnectivityString(ctrl_state_msg.serverCommStatus, g_lang));
  }

  wifiVisible = false;
  hmi_msg.shouldSendData = true;
}

void LanguageButton_cb(lv_event_t *e) {
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_InfoDetailsCont, LV_OBJ_FLAG_HIDDEN);
  wifiVisible = false;
  lv_obj_clear_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  LanguagesVisible = true;
  hmi_msg.shouldSendData = true;
}

void TextArea_Change_cb(lv_event_t *e) {
  lv_obj_t *ta = lv_event_get_target(e);
  const char *txt = lv_textarea_get_text(ta);
  if (ta == ui_TextArea1) {
    strncpy(wifi_ssid, txt, sizeof(wifi_ssid) - 1);
    wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';
  } else if (ta == ui_TextArea2) {
    strncpy(wifi_pass, txt, sizeof(wifi_pass) - 1);
    wifi_pass[sizeof(wifi_pass) - 1] = '\0';
  }
}

void TextArea_focus_cb(lv_event_t *e) {
  lv_obj_t *ta = lv_event_get_target(e);
  lv_keyboard_set_textarea(ui_Keyboard1, ta);
  lv_obj_clear_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectButton, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiDisconnectButton, LV_OBJ_FLAG_HIDDEN);
}

void Keyboard_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
    const char *txt1 = lv_textarea_get_text(ui_TextArea1);
    const char *txt2 = lv_textarea_get_text(ui_TextArea2);
    strncpy(wifi_ssid, txt1, sizeof(wifi_ssid));
    strncpy(wifi_pass, txt2, sizeof(wifi_pass));
    updateButtonVisibility();
  }
}

void AirPanel_cb(lv_event_t *e) {
  if (!tempSwitched)
    return;
  selectedPanel = AIR_PANEL_SELECTED;
  lastSelectedPanel = selectedPanel;
  set_active_panel(ui_AirPanel, ui_SkinPanel);

  // Visibility
  lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);

  // Lock Screen Sync (if objects exist)
  if (ui_AirTempLockCont)
    lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetAirTempCont)
    lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_SkinTempLockCont)
    lv_obj_add_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetSkinTempCont)
    lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);

  hmi_msg.controlMode = CONTROL_AIR;
  hmi_msg.shouldSendData = true;
  temp_chart_show_for_selected_panel();
}

void SkinPanel_cb(lv_event_t *e) {
  if (!tempSwitched)
    return;
  if (!skinPanelEnabled)
    return;
  selectedPanel = SKIN_PANEL_SELECTED;
  lastSelectedPanel = selectedPanel;
  set_active_panel(ui_SkinPanel, ui_AirPanel);

  // Visibility
  lv_obj_clear_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);

  // Lock Screen Sync
  if (ui_SkinTempLockCont)
    lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetSkinTempCont)
    lv_obj_clear_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_AirTempLockCont)
    lv_obj_add_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetAirTempCont)
    lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);

  hmi_msg.controlMode = CONTROL_SKIN;
  hmi_msg.shouldSendData = true;
  if (g_autoAirActive)
    autoair_deactivate(true); // ARQ-AUTOAIR-004: auto-deactivate on mode switch
  temp_chart_show_for_selected_panel();
}

void PhotoTimeMinusBtn_cb(lv_event_t *e) {
  hmi_msg.shouldSendData = true;
  if (photoTimerActive)
    return;

  if (photoTimerMinutes > 120) {
    photoTimerMinutes -= 20;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "%d:%02d", photoTimerMinutes / 60,
           photoTimerMinutes % 60);
  lv_label_set_text(ui_PhotoTimeValueLabel, buf);

  // Persist last used timer
  EEPROM.write(EEPROM_PHOTO_TIMER_MINUTES, photoTimerMinutes);
  eepromDirty = true;
  lastVarChangeTime = millis();
}

void PhotoTimePlusBtn_cb(lv_event_t *e) {
  hmi_msg.shouldSendData = true;
  if (photoTimerActive)
    return;

  if (photoTimerMinutes < 600) {
    photoTimerMinutes += 20;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "%d:%02d", photoTimerMinutes / 60,
           photoTimerMinutes % 60);
  lv_label_set_text(ui_PhotoTimeValueLabel, buf);

  // Persist last used timer
  EEPROM.write(EEPROM_PHOTO_TIMER_MINUTES, photoTimerMinutes);
  eepromDirty = true;
  lastVarChangeTime = millis();
}

void PhotoStartBtn_cb(lv_event_t *e) {
  if (photoTimerActive)
    return;

  photoTimerActive = true;
  photoTimerStartMs = millis();

  // Update visual state via SyncAll
  UI_SyncAll();

  hmi_msg.photoMinutesRemaining = photoTimerMinutes;
  hmi_msg.shouldSendData = true;
}

void PhotoCancelBtn_cb(lv_event_t *e) {
  photoTimerActive = false;

  // Notify Motherboard to stop timer but keep light ON (Continuous mode)
  hmi_msg.photoMinutesRemaining = 0;
  hmi_msg.shouldSendData = true;

  // Update visual state via SyncAll (this handles colors, labels and "X"
  // visibility)
  UI_SyncAll();
}

/* Switch callback for temperature and humidity */
void Switch_cb(lv_event_t *e) {
  lv_color_t active_col = darkMode ? COLOR_PANEL_GRAY : COLOR_PANEL_WHITE;
  lv_color_t inactive_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_GRAY;
  lv_obj_t *obj = lv_event_get_target(e); // switch that triggered the event
  lv_obj_t *panel = NULL;

  bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);

  if (obj == ui_Switch1) { // TEMPERATURE SWITCH
    switchTemp = checked;
    tempSwitched = checked;
    panel = ui_Panel1;

    if (checked) { // Temperature switch turned ON
      // mark last pressed chart and show temp page
      chartLastPressed = 0;
      lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_ON);

      // Gray out both panels initially
      if (lastSelectedPanel == AIR_PANEL_SELECTED) {
        selectedPanel = AIR_PANEL_SELECTED;
        set_active_panel(ui_AirPanel, ui_SkinPanel);
        hmi_msg.controlMode = CONTROL_AIR;
      } else if (lastSelectedPanel == SKIN_PANEL_SELECTED) {
        selectedPanel = SKIN_PANEL_SELECTED;
        set_active_panel(ui_SkinPanel, ui_AirPanel);
        hmi_msg.controlMode = CONTROL_SKIN;
        if (g_autoAirActive)
          autoair_deactivate(true); // ARQ-AUTOAIR-004
      } else { // No previous panel, default to Air
        selectedPanel = AIR_PANEL_SELECTED;
        set_active_panel(ui_AirPanel, ui_SkinPanel);
        hmi_msg.controlMode = CONTROL_AIR;
      }

      temp_chart_show_for_selected_panel();

      // Enable temperature arrows
      arrowsActive = true;
      lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_color(ui_ArrowDownTemp, active_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_ArrowUpTemp, active_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_Panel1, active_col, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);
    } else { // Temperature switch turned OFF
      selectedPanel = NO_PANEL_SELECTED;
      lv_obj_add_flag(ui_AirTempChartCont,
                      LV_OBJ_FLAG_HIDDEN); // hide temp chart
      lv_obj_add_flag(ui_SkinTempChartCont,
                      LV_OBJ_FLAG_HIDDEN); // hide skin temp chart
      arrowsActive = false;

      // If humidity is ON, switch to humidity chart
      if (switchHum) {
        chartLastPressed = 1;
        lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_ON);
        lv_obj_clear_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
      } else {
        // no charts active
        lv_obj_add_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
        chartLastPressed = -1;
      }

      // Disable temperature arrows
      lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_color(ui_ArrowDownTemp, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_ArrowUpTemp, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_Panel1, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);

      // Gray out both panels
      lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
      lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);
    }
  } else if (obj == ui_Switch2) { // HUMIDITY SWITCH
    switchHum = checked;
    humSwitched = checked;
    panel = ui_Panel3;

    if (checked) {
      // mark last pressed chart and show hum page
      chartLastPressed = 1;
      lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_ON);
      lv_obj_clear_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN); // show hum

      // Show humidity target in lock screen
      lv_obj_clear_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);

      // Enable humidity arrows
      lv_obj_add_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_color(ui_ArrowDownHum, active_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_ArrowUpHum, active_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_Panel3, active_col, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);
    } else {
      lv_obj_add_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN); // hide hum chart
      // Humidity OFF
      lv_obj_clear_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_color(ui_ArrowDownHum, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_ArrowUpHum, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_Panel3, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);

      lv_obj_set_style_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);

      // If temperature is ON, switch to temperature chart
      if (switchTemp) {
        chartLastPressed = 0;
        lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_ON);
        lv_obj_clear_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
        chartLastPressed = -1;
      }
      // Hide humidity target when humidity turned off
      lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
    }
  } else if (obj == ui_Switch3) { // PHOTOTHERAPY SWITCH
    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);

    if (checked) {
      // Active state on Motherboard immediately
      hmi_msg.phototherapyMode = PHOTOTHERAPY_ON;
      hmi_msg.shouldSendData = true;

      lv_obj_set_style_bg_color(ui_PhotoTimerPanel, active_col, LV_PART_MAIN);
      lv_obj_add_flag(ui_PhotoTimeMinusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(ui_PhotoTimePlusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(ui_PhotoStartBtn, LV_OBJ_FLAG_CLICKABLE);

      // Restore button colors
      lv_obj_set_style_bg_color(ui_PhotoTimeMinusBtn, lv_color_hex(0x0075EE),
                                LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_PhotoTimePlusBtn, lv_color_hex(0x0075EE),
                                LV_PART_MAIN);

      // Reset timer UI if no timer is running
      if (!photoTimerActive) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d", photoTimerMinutes / 60,
                 photoTimerMinutes % 60);
        lv_label_set_text(ui_PhotoTimeValueLabel, buf);

        // Use localized text
        const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
        lv_label_set_text(ui_PhotoStartLabel, TXT_PHOTOSTART[g_lang]);
        lv_obj_set_style_bg_color(ui_PhotoStartBtn, lv_color_hex(0x00AA00),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

        // Signal CONTINUOUS mode to Motherboard (Minutes = 0)
        hmi_msg.photoMinutesRemaining = 0;
        hmi_msg.shouldSendData = true;
      } else {
        // If already running (e.g. skin/air change while timer was active)
        lv_obj_set_style_bg_color(ui_PhotoStartBtn, lv_color_hex(0x888888),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
      }
    } else {
      // OFF / Grayed out
      hmi_msg.phototherapyMode = PHOTOTHERAPY_OFF;
      hmi_msg.shouldSendData = true;

      lv_obj_set_style_bg_color(ui_PhotoTimerPanel, inactive_col, LV_PART_MAIN);
      lv_obj_clear_flag(ui_PhotoTimeMinusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_PhotoTimePlusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_PhotoStartBtn, LV_OBJ_FLAG_CLICKABLE);

      // Buttons to light gray as requested
      lv_obj_set_style_bg_color(ui_PhotoTimeMinusBtn, COLOR_PANEL_LIGHT_GRAY,
                                LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_PhotoTimePlusBtn, COLOR_PANEL_LIGHT_GRAY,
                                LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_PhotoStartBtn, COLOR_PANEL_LIGHT_GRAY,
                                LV_PART_MAIN);

      // Reset visual values when turned OFF (but keep the stored minutes)
      char buf[16];
      snprintf(buf, sizeof(buf), "%d min", photoTimerMinutes);
      lv_label_set_text(ui_PhotoTimeValueLabel, buf);

      const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
      lv_label_set_text(ui_PhotoStartLabel, TXT_PHOTOSTART[g_lang]);

      if (ui_PhotoCancelBtn)
        lv_obj_add_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);

      photoTimerActive = false;
    }
  } else if (obj == ui_Switch4) { // SKIN BLOCK SWITCH
    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);

    // RF-SKIN-007/009: Block skin mode if probe not valid (ARQ-SKIN-002/003)
    if (checked && g_skinProbeState != SKIN_PROBE_VALID) {
      // Revert the switch visually without triggering callback again
      ui_set_switch_state_silent(ui_Switch4, false);
      // RF-SKIN-010/UI-SKIN-004: Show clear message to user
      if (ui_SkinProbeToast) {
        const char *msg =
            (g_lang == LANG_ES)
                ? "Modo piel no disponible:\nConecte la sonda de temperatura"
            : (g_lang == LANG_FR)
                ? "Mode peau indisponible:\nConnectez la sonde de temperature"
                : "Skin mode unavailable:\nConnect the skin temperature probe";
        lv_label_set_text(ui_SkinProbeToast, msg);
        lv_obj_clear_flag(ui_SkinProbeToast, LV_OBJ_FLAG_HIDDEN);
        lv_timer_create(
            [](lv_timer_t *t) {
              if (ui_SkinProbeToast)
                lv_obj_add_flag(ui_SkinProbeToast, LV_OBJ_FLAG_HIDDEN);
              lv_timer_del(t);
            },
            4000, nullptr);
      }
      ESP_LOGW(TAG,
               "[SKIN-PROBE] Blocked skin mode activation - probe state=%d",
               g_skinProbeState);
      return;
    }

    skinPanelEnabled = checked;
    hmi_msg.skinModeEnabled = checked;
    hmi_msg.shouldSendData = true;

    if (checked) {
      // show container of skin
      lv_obj_clear_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);

      lv_obj_set_style_bg_color(ui_SkinPanelCont, COLOR_PANEL_WHITE,
                                LV_PART_MAIN);
      lv_obj_set_style_opa(ui_SkinPanelCont, LV_OPA_COVER, LV_PART_MAIN);
    } else {
      // Hide container of skin
      lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
      lastSelectedPanel = AIR_PANEL_SELECTED;
      lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);

      if (selectedPanel == SKIN_PANEL_SELECTED) {
        selectedPanel = AIR_PANEL_SELECTED;
        lastSelectedPanel = selectedPanel;

        // Switch active panel to Air
        set_active_panel(ui_AirPanel, ui_SkinPanel);

        // Visibility restore
        lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);

        if (ui_AirTempLockCont)
          lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
        if (ui_TargetAirTempCont)
          lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
        if (ui_SkinTempLockCont)
          lv_obj_add_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
        if (ui_TargetSkinTempCont)
          lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);

        // Update control mode if temperature is switched on
        if (tempSwitched) { // only if temp is ON
          hmi_msg.controlMode = CONTROL_AIR;
          temp_chart_show_for_selected_panel();
        }
      }
    }
  } else if (obj == ui_SwitchDarkMode) { // DARK MODE SWITCH
    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);
    darkMode = checked;
    EEPROM.write(EEPROM_DARK_MODE, darkMode ? 1 : 0);
    eepromDirty = true;
    lastVarChangeTime = millis();
    UI_ApplyTheme();
  }

  // If temperature is OFF, disable panels and arrows (por si acaso)
  if (!tempSwitched) {
    arrowsActive = false;

    lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
  } else {
    // If a panel is active, enable arrows
    if (selectedPanel != NO_PANEL_SELECTED) {
      arrowsActive = true;
      lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    } else {
      arrowsActive = false;
      lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    }
  }

  // Change background color of associated panel
  if (panel != NULL) {
    if (checked) {
      lv_obj_set_style_bg_color(panel, COLOR_PANEL_WHITE, LV_PART_MAIN);
      lv_obj_set_style_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_color(panel, COLOR_PANEL_GRAY, LV_PART_MAIN);
      lv_obj_set_style_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    }
  }

  // Manage tabview and tab buttons so that when only one switch is ON
  // the UI behaves as if only a single tab exists (no header/button to switch).
  lv_obj_t *tab_btns_cont =
      lv_obj_get_child(ui_TabView1, 0); // header container (may be NULL)
  lv_obj_t *temp_tab_btn = NULL;
  lv_obj_t *hum_tab_btn = NULL;
  if (tab_btns_cont) {
    temp_tab_btn = lv_obj_get_child(tab_btns_cont, 0);
    hum_tab_btn = lv_obj_get_child(tab_btns_cont, 1);
  }

  if (!switchTemp && !switchHum) {
    // No charts: hide entire tabview
    lv_obj_add_flag(ui_TabView1, LV_OBJ_FLAG_HIDDEN);
  } else {
    // Ensure tabview visible
    lv_obj_clear_flag(ui_TabView1, LV_OBJ_FLAG_HIDDEN);

    // Show/hide individual tab buttons (labels)
    if (temp_tab_btn) {
      if (switchTemp)
        lv_obj_clear_flag(temp_tab_btn, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(temp_tab_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (hum_tab_btn) {
      if (switchHum)
        lv_obj_clear_flag(hum_tab_btn, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(hum_tab_btn, LV_OBJ_FLAG_HIDDEN);
    }

    // If exactly one chart is visible, hide the header container
    if ((switchTemp && !switchHum) || (!switchTemp && switchHum)) {
      if (tab_btns_cont)
        lv_obj_add_flag(tab_btns_cont, LV_OBJ_FLAG_HIDDEN);
    } else {
      if (tab_btns_cont)
        lv_obj_clear_flag(tab_btns_cont, LV_OBJ_FLAG_HIDDEN);
    }

    // Select active tab: single visible -> that tab; both visible -> respect
    // chartLastPressed
    if (switchTemp && !switchHum) {
      lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_ON);
    } else if (!switchTemp && switchHum) {
      lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_ON);
    } else if (switchTemp && switchHum) {
      if (chartLastPressed == 1)
        lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_ON);
      else
        lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_ON);
    }
  }

  // --- Actuation mode selection logic ---
  if (switchTemp && switchHum) {
    hmi_msg.actuation = ACTUATION_TEMP_AND_HUMIDITY;
  } else if (switchTemp) {
    hmi_msg.actuation = ACTUATION_TEMPERATURE;
  } else if (switchHum) {
    hmi_msg.actuation = ACTUATION_HUMIDITY;
  } else {
    hmi_msg.actuation = ACTUATION_NONE;
  }

  hmi_msg.desiredAirTemperature = airTempValue;
  hmi_msg.desiredSkinTemperature = skinTempValue;
  hmi_msg.desiredHumidity = humValue;
  hmi_msg.shouldSendData = true;

  update_labels();
  UI_SyncAll();
}

void setup_panel_callbacks() {
  lv_obj_add_event_cb(ui_AirPanel, AirPanel_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_SkinPanel, SkinPanel_cb, LV_EVENT_CLICKED, NULL);
}

void setup_arrow_callbacks() {
  lv_obj_add_event_cb(
      ui_ImgArrowUpTemp,
      [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t *target = lv_event_get_target(e);

        if (code == LV_EVENT_PRESSED) {
          lv_obj_set_style_transform_zoom(target, 280,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_CLICKED) {
          if (!tempSwitched)
            return;
          if (selectedPanel == AIR_PANEL_SELECTED) {
            airTempValue += TEMP_INCREMENT;
            if (airTempValue > AIR_TEMP_MAX)
              airTempValue = AIR_TEMP_MAX;
            hmi_msg.desiredAirTemperature = airTempValue;
            EEPROM.writeFloat(EEPROM_DESIRED_AIR_TEMP, airTempValue);
          } else if (selectedPanel == SKIN_PANEL_SELECTED) {
            skinTempValue += TEMP_INCREMENT;
            if (skinTempValue > SKIN_TEMP_MAX)
              skinTempValue = SKIN_TEMP_MAX;
            hmi_msg.desiredSkinTemperature = skinTempValue;
            EEPROM.writeFloat(EEPROM_DESIRED_SKIN_TEMP, skinTempValue);
          }
          hmi_msg.shouldSendData = true;
          eepromDirty = true;
          lastVarChangeTime = millis();
          update_labels();
        }
      },
      LV_EVENT_ALL, NULL);

  lv_obj_add_event_cb(
      ui_ImgArrowDownTemp,
      [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t *target = lv_event_get_target(e);

        if (code == LV_EVENT_PRESSED) {
          lv_obj_set_style_transform_zoom(target, 280,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_CLICKED) {
          if (!tempSwitched)
            return;
          if (selectedPanel == AIR_PANEL_SELECTED) {
            airTempValue -= TEMP_INCREMENT;
            if (airTempValue < AIR_TEMP_MIN)
              airTempValue = AIR_TEMP_MIN;
            hmi_msg.desiredAirTemperature = airTempValue;
            EEPROM.writeFloat(EEPROM_DESIRED_AIR_TEMP, airTempValue);
          } else if (selectedPanel == SKIN_PANEL_SELECTED) {
            skinTempValue -= TEMP_INCREMENT;
            if (skinTempValue < SKIN_TEMP_MIN)
              skinTempValue = SKIN_TEMP_MIN;
            hmi_msg.desiredSkinTemperature = skinTempValue;
            EEPROM.writeFloat(EEPROM_DESIRED_SKIN_TEMP, skinTempValue);
          }
          hmi_msg.shouldSendData = true;
          eepromDirty = true;
          lastVarChangeTime = millis();
          update_labels();
        }
      },
      LV_EVENT_ALL, NULL);
}

void setup_arrow_hum_callbacks() {
  lv_obj_add_event_cb(
      ui_ImgArrowUpHum,
      [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t *target = lv_event_get_target(e);

        if (code == LV_EVENT_PRESSED) {
          lv_obj_set_style_transform_zoom(target, 280,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_CLICKED) {
          if (!switchHum)
            return;
          humValue += HUM_STEP;
          if (humValue > HUM_MAX)
            humValue = HUM_MAX;
          hmi_msg.desiredHumidity = humValue;
          hmi_msg.shouldSendData = true;
          EEPROM.write(EEPROM_DESIRED_HUMIDITY, humValue);
          eepromDirty = true;
          lastVarChangeTime = millis();
          update_labels();
        }
      },
      LV_EVENT_ALL, NULL);

  lv_obj_add_event_cb(
      ui_ImgArrowDownHum,
      [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t *target = lv_event_get_target(e);

        if (code == LV_EVENT_PRESSED) {
          lv_obj_set_style_transform_zoom(target, 280,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_CLICKED) {
          if (!switchHum)
            return;
          humValue -= HUM_STEP;
          if (humValue < HUM_MIN)
            humValue = HUM_MIN;
          hmi_msg.desiredHumidity = humValue;
          hmi_msg.shouldSendData = true;
          EEPROM.write(EEPROM_DESIRED_HUMIDITY, humValue);
          eepromDirty = true;
          lastVarChangeTime = millis();
          update_labels();
        }
      },
      LV_EVENT_ALL, NULL);
}

void blink_cb(void *obj, int32_t v) {
  lv_obj_set_style_bg_opa((lv_obj_t *)obj, v, LV_PART_MAIN);
}

void start_alarm_blink(lv_obj_t *obj) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_40);
  lv_anim_set_time(&a, ANIM_TIME_MS);
  lv_anim_set_playback_time(&a, ANIM_PLAYBACK_MS);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, blink_cb);
  lv_anim_start(&a);
}

void update_alarm_panels() {
  int totalActiveAlarms = 0;
  int activeCount = 0;
  alarmSlotToIndex[0] = -1;
  alarmSlotToIndex[1] = -1;
  alarmSlotToIndex[2] = -1;
  alarmSlotToIndex[3] = -1;

  bool selectedStillActive = false;

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarmList[i].state) {
      totalActiveAlarms++;
      if (activeCount < MAX_ALARM_DISPLAY) {
        alarmSlotToIndex[activeCount] = i;
        activeCount++;
      }
      if (alarmList[i].id == g_selectedAlarmId) {
        selectedStillActive = true;
      }
    }
  }

  // Limpiar descripción de alarma si ya no está activa (Casos A y C)
  if (g_selectedAlarmId != -1 && !selectedStillActive) {
    if (ui_AlarmDetailLabel)
      lv_label_set_text(ui_AlarmDetailLabel, "");
    g_selectedAlarmId = -1;
  }

  alarmActive = (totalActiveAlarms > 0);
  hmi_msg.muteAlarm = alarmActive ? (alarmsMuted ? 1 : 0) : 0;
  // hmi_msg.shouldSendData = true; Dont send reply when receive an alarm msg

  if (activeCount > NUM_ALARM_0) {
    lv_obj_clear_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN);
    int idx = alarmSlotToIndex[0];
    lv_label_set_text(ui_Alarm1Label, alarmList[idx].type);
    start_alarm_blink(ui_Alarm1Panel);
  }
  if (activeCount > NUM_ALARM_1) {
    lv_obj_clear_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN);
    int idx = alarmSlotToIndex[1];
    lv_label_set_text(ui_Alarm2Label, alarmList[idx].type);
    start_alarm_blink(ui_Alarm2Panel);
  }
  if (activeCount > NUM_ALARM_2) {
    lv_obj_clear_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN);
    int idx = alarmSlotToIndex[2];
    lv_label_set_text(ui_Alarm3Label, alarmList[idx].type);
    start_alarm_blink(ui_Alarm3Panel);
  }
  if (activeCount > NUM_ALARM_3) {
    lv_obj_clear_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN);
    int idx = alarmSlotToIndex[3];
    lv_label_set_text(ui_Alarm4Label, alarmList[idx].type);
    start_alarm_blink(ui_Alarm4Panel);
  }

  int pos = activeCount;
  if (pos < MAX_ALARM_DISPLAY) {
    if (pos <= NUM_ALARM_0)
      lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN);
    if (pos <= NUM_ALARM_1)
      lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN);
    if (pos <= NUM_ALARM_2)
      lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN);
    if (pos <= NUM_ALARM_3)
      lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN);
  }

  if (totalActiveAlarms > 0) {
    char buf[10];
    itoa(totalActiveAlarms, buf, 10);
    // Main screen
    lv_obj_clear_flag(ui_AlarmButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_NumAlarm, buf);
    lv_obj_add_flag(ui_CheckImgMain, LV_OBJ_FLAG_HIDDEN);
    // Lock screen
    lv_obj_clear_flag(ui_AlarmLockCont, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_AlarmLockNumLabel, buf);
    lv_obj_add_flag(ui_CheckImg, LV_OBJ_FLAG_HIDDEN);
  } else {
    // Main screen
    lv_obj_add_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_AlarmButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_CheckImgMain, LV_OBJ_FLAG_HIDDEN);
    // Lock screen
    lv_obj_add_flag(ui_AlarmLockCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_CheckImg, LV_OBJ_FLAG_HIDDEN);
  }

  if (totalActiveAlarms == 0) {
    alarmsMuted = false;
  }

  // Handle Heater Error logic
  static bool heaterCriticalError = false; // Latching variable

  if (!heaterCriticalError) {
    for (int i = 0; i < MAX_ALARMS; i++) {
      if (alarmList[i].id == HEATER_ISSUE_ALARM && alarmList[i].state) {
        heaterCriticalError = true;
        break;
      }
    }
  }

  if (heaterCriticalError) {
    // Show Warning UI
    if (ui_HeaterErrorTempCont)
      lv_obj_clear_flag(ui_HeaterErrorTempCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_HeaterErrorHumCont)
      lv_obj_clear_flag(ui_HeaterErrorHumCont, LV_OBJ_FLAG_HIDDEN);

    // Disable Switches
    if (ui_Switch1) {
      lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
      lv_obj_add_state(ui_Switch1, LV_STATE_DISABLED);
    }
    if (ui_Switch2) {
      lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
      lv_obj_add_state(ui_Switch2, LV_STATE_DISABLED);
    }

    // Blink - Blink the CONTAINER for visibility
    if (ui_HeaterErrorTempCont)
      start_alarm_blink(ui_HeaterErrorTempCont);
    if (ui_HeaterErrorHumCont)
      start_alarm_blink(ui_HeaterErrorHumCont);

  } else {
    // Hide Warning UI
    if (ui_HeaterErrorTempCont)
      lv_obj_add_flag(ui_HeaterErrorTempCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_HeaterErrorHumCont)
      lv_obj_add_flag(ui_HeaterErrorHumCont, LV_OBJ_FLAG_HIDDEN);

    // Enable Switches
    if (ui_Switch1)
      lv_obj_clear_state(ui_Switch1, LV_STATE_DISABLED);
    if (ui_Switch2)
      lv_obj_clear_state(ui_Switch2, LV_STATE_DISABLED);
  }
}

// Event handler for Heater Error Click
static void HeaterError_event_handler(lv_event_t *e) {
  // Go to Alarms Screen
  _ui_screen_change(&ui_ScreenAlarms, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenAlarms_screen_init);
  lv_tabview_set_act(ui_AlarmsTabview, 1,
                     LV_ANIM_OFF); // Go to Description Tab (Page 1)

  // Set specific description
  const char *TXT_HEATER_ERROR_DESC[] = {
      "Hay un problema con el ventilador / calefactor: \n\n 1. Pruebe a "
      "desconectar y conectar el calefactor y posteriormente reinicie "
      "incubadora\n\n 2. Si no funciona el paso 1, arregle el calefactor.",
      "There is a problem with the fan / heater:\n\n 1. Try unplugging and "
      "plugging in the heater and then restart the incubator\n\n 2. If step 1 "
      "doesn't work, fix the heater.",
      "Il y a un probleme avec le ventilateur / chauffage :\n\n 1. Essayez de "
      "debrancher et de brancher le chauffage et redemarrez l'incubateur\n\n "
      "2. Si le pas 1 ne fonctionne pas, reparez le chauffage."};
  if (ui_AlarmDetailLabel) {
    lv_label_set_text(ui_AlarmDetailLabel, TXT_HEATER_ERROR_DESC[g_lang]);
    g_selectedAlarmId = HEATER_ISSUE_ALARM;
  }
}

void show_alarm_detail_from_slot(int slot) {
  if (slot < 0 || slot >= MAX_ALARM_DISPLAY)
    return;
  int idx = alarmSlotToIndex[slot];
  if (idx < 0)
    return;
  lv_tabview_set_act(ui_AlarmsTabview, 1, LV_ANIM_ON);
  lv_label_set_text(ui_AlarmDetailLabel, alarmList[idx].description);
  g_selectedAlarmId = alarmList[idx].id;
}

void Alarm1Cont_cb(lv_event_t *e) {
  show_alarm_detail_from_slot(0);
  hmi_msg.shouldSendData = true;
}
void Alarm2Cont_cb(lv_event_t *e) {
  show_alarm_detail_from_slot(1);
  hmi_msg.shouldSendData = true;
}
void Alarm3Cont_cb(lv_event_t *e) {
  show_alarm_detail_from_slot(2);
  hmi_msg.shouldSendData = true;
}
void Alarm4Cont_cb(lv_event_t *e) {
  show_alarm_detail_from_slot(3);
  hmi_msg.shouldSendData = true;
}

void reset_alarm_detail_state() {
  if (ui_AlarmDetailLabel)
    lv_label_set_text(ui_AlarmDetailLabel, "");
  g_selectedAlarmId = -1;
}

void AlarmButton_cb(lv_event_t *e) {
  reset_alarm_detail_state();
  lv_tabview_set_act(ui_AlarmsTabview, 0, LV_ANIM_ON);
}

void AlarmsTabview_cb(lv_event_t *e) {
  if (!ui_AlarmsTabview)
    return;
  uint16_t act = lv_tabview_get_tab_act(ui_AlarmsTabview);
  if (act == 0) { // Si volvemos a la lista de alarmas
    reset_alarm_detail_state();
  }
  hmi_msg.shouldSendData = true;
}

void chart_add_hum_value(float hum) {
  if (humSeries == NULL)
    return;
  if (hum < 0)
    hum = 0;
  if (hum > 100)
    hum = 100;
  lv_chart_set_next_value(ui_HumChart, humSeries, (lv_coord_t)hum);
}

void chart_save_history() {
  decimationCounter++;
  if (decimationCounter >= 10) { // 1 cada 10 muestras (~10 segundos)
    decimationCounter = 0;
    historyBufferAir[historyWriteIdx] = (float)airTempValueDetected;
    historyBufferSkin[historyWriteIdx] = (float)skinTempValueDetected;
    historyBufferHum[historyWriteIdx] = (float)humValueDetected;

    historyWriteIdx = (historyWriteIdx + 1) % HISTORY_BUFFER_SIZE;
    if (historySampleCount < HISTORY_BUFFER_SIZE)
      historySampleCount++;

    // Si la pantalla de gráficas está activa, refrescar el historial
    if (lv_scr_act() == ui_ScreenCharts) {
      update_history_charts();
    }
  }
}

void update_history_charts() {
  if (!ui_HistoryChartAire || !historySeriesAire)
    return;
  if (!ui_HistoryChartHum || !historySeriesHum)
    return;

  uint16_t interval_idx = lv_dropdown_get_selected(ui_HistoryDropdown);
  int point_count = 0;
  switch (interval_idx) {
  case 0:
    point_count = 30;
    break; // 5 min (~5 min @ 10s/sample)
  case 1:
    point_count = 180;
    break; // 30 min
  case 2:
    point_count = 360;
    break; // 1 h
  case 3:
    point_count = 720;
    break; // 2 h
  default:
    point_count = 30;
  }

  // Si hay menos datos que los pedidos, mostrar solo los disponibles
  if (point_count > historySampleCount)
    point_count = historySampleCount;

  // Si no hay datos aún, no dibujar nada
  if (point_count == 0)
    return;

  // ---- Temperatura Aire ----
  // Reset de la serie: poner todos los puntos como NONE
  lv_chart_set_point_count(ui_HistoryChartAire, point_count);
  for (int i = 0; i < point_count; i++)
    historySeriesAire->y_points[i] = LV_CHART_POINT_NONE;
  historySeriesAire->start_point = 0;

  // ---- Temperatura Piel ----
  lv_chart_set_point_count(ui_HistoryChartSkin, point_count);
  for (int i = 0; i < point_count; i++)
    historySeriesSkin->y_points[i] = LV_CHART_POINT_NONE;
  historySeriesSkin->start_point = 0;

  // ---- Humedad ----
  lv_chart_set_point_count(ui_HistoryChartHum, point_count);
  for (int i = 0; i < point_count; i++)
    historySeriesHum->y_points[i] = LV_CHART_POINT_NONE;
  historySeriesHum->start_point = 0;

  // Calcular índice de inicio en el búfer circular
  int start_idx = (historyWriteIdx - point_count + HISTORY_BUFFER_SIZE) %
                  HISTORY_BUFFER_SIZE;

  // Cargar datos del búfer secuencialmente en las series
  for (int i = 0; i < point_count; i++) {
    int buf_idx = (start_idx + i) % HISTORY_BUFFER_SIZE;
    lv_chart_set_next_value(ui_HistoryChartAire, historySeriesAire,
                            (lv_coord_t)historyBufferAir[buf_idx]);
    lv_chart_set_next_value(ui_HistoryChartSkin, historySeriesSkin,
                            (lv_coord_t)historyBufferSkin[buf_idx]);
    lv_chart_set_next_value(ui_HistoryChartHum, historySeriesHum,
                            (lv_coord_t)historyBufferHum[buf_idx]);
  }

  lv_chart_refresh(ui_HistoryChartAire);
  lv_chart_refresh(ui_HistoryChartSkin);
  lv_chart_refresh(ui_HistoryChartHum);

  // Los valores individuales (Aire/Piel/Hum) se actualizan en update_labels()
}

void update_history_charts();

void TabViewHistory_cb(lv_event_t *e) { update_history_charts(); }

void ScreenCharts_load_cb(lv_event_t *e) { update_history_charts(); }

void HistoryDropdown_cb(lv_event_t *e) { update_history_charts(); }

void AlarmSound_Update() {
  // if (alarmActive && !alarmsMuted)
  //   buzzerOn();
  // else
  //  El display ya no emite sonido por alarmas, solo la motherboard.
  buzzerOff();
}

void MuteAlarm_cb(lv_event_t *e) {
  (void)e;
  alarmsMuted = true;
  hmi_msg.muteAlarm = 1;
  hmi_msg.shouldSendData = true;
  lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);
  AlarmSound_Update();
}

static void show_targets_for_mode(void) {
  if (unlockTimeoutTimer) {
    lv_timer_del(unlockTimeoutTimer);
    unlockTimeoutTimer = NULL;
  }
  lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowHumLock, LV_OBJ_FLAG_HIDDEN);

  if (tempSwitched) {
    if (selectedPanel == AIR_PANEL_SELECTED) {
      lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
    } else if (selectedPanel == SKIN_PANEL_SELECTED) {
      lv_obj_clear_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (switchHum) {
    lv_obj_clear_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ArrowHumLock, LV_OBJ_FLAG_HIDDEN);
  }

  // FIRST COLUMN (Detected Values) ALWAYS VISIBLE
  if (ui_AirTempLockCont)
    lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_SkinTempLockCont)
    lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_HumLockCont)
    lv_obj_clear_flag(ui_HumLockCont, LV_OBJ_FLAG_HIDDEN);
}

static void unlock_timeout_cb(lv_timer_t *t) {
  (void)t;
  show_targets_for_mode();
  if (unlockTimeoutTimer) {
    lv_timer_del(unlockTimeoutTimer);
    unlockTimeoutTimer = NULL;
  }
}

static void show_unlock_only(void) {
  lv_obj_clear_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);

  // Start or reset inactivity timer to hide UnlockCont
  if (unlockTimeoutTimer) {
    lv_timer_reset(unlockTimeoutTimer);
  } else {
    unlockTimeoutTimer =
        lv_timer_create(unlock_timeout_cb, UNLOCK_TIMEOUT_MS, NULL);
    lv_timer_set_repeat_count(unlockTimeoutTimer, 1);
  }
}

static void lock_progress_timer_cb(lv_timer_t *t) {
  (void)t;
  if (!lockProgressArc)
    return;
  uint32_t now = lv_tick_get();
  uint32_t elapsed = now - lockProgressStart;
  if (elapsed > LOCK_PROGRESS_DURATION_MS)
    elapsed = LOCK_PROGRESS_DURATION_MS;
  int perc = (int)((elapsed * 100) / LOCK_PROGRESS_DURATION_MS);
  lv_arc_set_value(lockProgressArc, perc);

  if (elapsed >= LOCK_PROGRESS_DURATION_MS) {
    if (lockProgressTimer) {
      lv_timer_del(lockProgressTimer);
      lockProgressTimer = NULL;
    }
    lv_scr_load(ui_ScreenMain);
    locked = false;
    if (lockProgressArc)
      lv_obj_add_flag(lockProgressArc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  }
}

static void start_lock_progress(void) {
  lockProgressArc = ui_Spinner1;
  if (!lockProgressArc)
    return;
  lv_obj_clear_flag(lockProgressArc, LV_OBJ_FLAG_HIDDEN);
  lv_arc_set_value(lockProgressArc, 0);
  lockProgressStart = lv_tick_get();
  if (lockProgressTimer) {
    lv_timer_del(lockProgressTimer);
    lockProgressTimer = NULL;
  }
  lockProgressTimer = lv_timer_create(lock_progress_timer_cb, 50, NULL);
}

static void stop_lock_progress(void) {
  if (lockProgressTimer) {
    lv_timer_del(lockProgressTimer);
    lockProgressTimer = NULL;
  }
  if (lockProgressArc) {
    lv_arc_set_value(lockProgressArc, 0);
    lv_obj_add_flag(lockProgressArc, LV_OBJ_FLAG_HIDDEN);
  }
}

static void enter_lock_screen(void) {
  if (lv_scr_act() == ui_ScreenLock) {
    stop_lock_progress();
    locked = true;
    show_targets_for_mode();
    return;
  }
  stop_lock_progress();
  locked = true;
  lv_scr_load(ui_ScreenLock);
  show_targets_for_mode();
  lv_disp_trig_activity(NULL);
}

void ImgButton1_Lock_cb(lv_event_t *e) {
  (void)e;
  if (lv_scr_act() == ui_ScreenMain) {
    enter_lock_screen();
  }
  hmi_msg.shouldSendData = true;
}

void LockScreenAnyTouch_cb(lv_event_t *e) {
  if (lv_scr_act() != ui_ScreenLock)
    return;
  lv_obj_t *origin = lv_event_get_target(e);
  if (origin != ui_ScreenLock)
    return;

  bool unlockVisible = !lv_obj_has_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);

  if (!unlockVisible) {
    show_unlock_only();
    locked = false;
  } else {
    stop_lock_progress();
    show_targets_for_mode();
    locked = true;
  }
}

static void UnlockCont_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    start_lock_progress();
    // Pause timeout timer while pressing
    if (unlockTimeoutTimer) {
      lv_timer_pause(unlockTimeoutTimer);
    }
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    stop_lock_progress();
    // Resume/Reset timeout timer when released
    if (unlockTimeoutTimer) {
      lv_timer_resume(unlockTimeoutTimer);
      lv_timer_reset(unlockTimeoutTimer);
    }
  }
}

static void add_unlock_press_cb_recursive(lv_obj_t *obj) {
  if (!obj)
    return;
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj, UnlockCont_event_cb, LV_EVENT_ALL, NULL);
  uint32_t n = lv_obj_get_child_cnt(obj);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t *child = lv_obj_get_child(obj, i);
    add_unlock_press_cb_recursive(child);
  }
}

void inactivity_timer_cb(lv_timer_t *timer) {
  if (lv_scr_act() == ui_ScreenAlarms) {
    lv_disp_trig_activity(NULL);
    return;
  }
  uint32_t inactive = lv_disp_get_inactive_time(NULL);
  if (inactive > INACTIVITY_TIMEOUT_MS) {
    if (lv_scr_act() != ui_ScreenLock) {
      stop_lock_progress();
      locked = true;
      lv_scr_load(ui_ScreenLock);
      show_targets_for_mode();
    }
  }
}

void WifiConnectButton_cb(lv_event_t *e) {
  hmi_msg.shouldSendData = true;
  extern char pendingSSID[64];
  extern char pendingPass[64];
  strncpy(pendingSSID, wifi_ssid, sizeof(pendingSSID));
  strncpy(pendingPass, wifi_pass, sizeof(pendingPass));

  Communication_SendWiFiCredentials(pendingSSID, pendingPass);
  vTaskDelay(
      pdMS_TO_TICKS(100)); // Ensure serial is clear before WiFi logs start
  wifiInit();              // Trigger new connection attempt
  isConnected = true;
  updateButtonVisibility();
}

void WifiDisconnectButton_cb(lv_event_t *e) {
  WiFi.disconnect();
  isConnected = false;
  pendingReconnect = true;
  disconnectTimestampMs = millis();
  updateButtonVisibility();
}

void Label9_cb(lv_event_t *e) {
  lv_obj_add_state(ui_Switch1, LV_STATE_CHECKED);
  lv_event_send(ui_Switch1, LV_EVENT_VALUE_CHANGED, NULL);
}

void Label15_cb(lv_event_t *e) {
  lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
  lv_event_send(ui_Switch1, LV_EVENT_VALUE_CHANGED, NULL);
}

void Label13_cb(lv_event_t *e) {
  lv_obj_add_state(ui_Switch2, LV_STATE_CHECKED);
  lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);
}

void Label16_cb(lv_event_t *e) {
  lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
  lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);
}

void Label10_cb(lv_event_t *e) {
  lv_obj_add_state(ui_Switch3, LV_STATE_CHECKED);
  lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);
}

void Label17_cb(lv_event_t *e) {
  lv_obj_clear_state(ui_Switch3, LV_STATE_CHECKED);
  lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);
}

void ui_set_switch_state_silent(lv_obj_t *sw, bool on) {
  if (!sw)
    return;
  if (on)
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  else
    lv_obj_clear_state(sw, LV_STATE_CHECKED);
}

void Settings_cb(lv_event_t *e) {
  reset_alarm_detail_state();
  _ui_screen_change(&ui_ScreenSettings, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenSettings_screen_init);
}

void SPO2Button_cb(lv_event_t *e) {
  reset_alarm_detail_state();
  _ui_screen_change(&ui_ScreenPulseOxi, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenPulseOxi_screen_init);
}

void ChartButton_cb(lv_event_t *e) {
  reset_alarm_detail_state();
  _ui_screen_change(&ui_ScreenCharts, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenCharts_screen_init);

  if (g_ui_initialized) {
    // Si la pantalla de charts se acaba de inicializar, podemos acceder a
    // ui_TabView1 Nota: ui_TabView1 se crea en ui_ScreenCharts_screen_init
    lv_tabview_set_act(ui_TabViewMainCharts, 0,
                       LV_ANIM_OFF); // Ir a Tiempo Real por defecto

    // Ocultar/Mostrar páginas del TabView según estado de sensores
    if (tempSwitched && humSwitched) {
      lv_tabview_set_act(ui_TabView1, 0,
                         LV_ANIM_OFF); // Mostrar Temp por defecto
    } else if (tempSwitched) {
      lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_OFF); // Forzar Temp
    } else if (humSwitched) {
      lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_OFF); // Forzar Hum
    }
    update_history_charts();
  }
}

void AlarmLockImg_cb(lv_event_t *e) {
  reset_alarm_detail_state();
  _ui_screen_delete(&ui_ScreenLock);
  _ui_screen_change(&ui_ScreenAlarms, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenAlarms_screen_init);
}

void AlarmLockCont_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    AlarmLockImg_cb(e);
  }
}

void ImgButton2_cb(lv_event_t *e) {
  _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenMain_screen_init);
  _ui_screen_delete(&ui_ScreenSettings);
}

void ImgButton7_cb(lv_event_t *e) {
  reset_alarm_detail_state();
  _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenMain_screen_init);
}

void ImgButton8_cb(lv_event_t *e) {
  _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenMain_screen_init);
}

void ImgButton9_cb(lv_event_t *e) {
  _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenMain_screen_init);
}

// ==========================================
// Callbacks
// ==========================================
void AudioTestBtn_cb(lv_event_t *e) {
  AudioManager::getInstance().playTone();
  if (ui_AudioPlayBtn) {
    lv_obj_add_state(ui_AudioPlayBtn, LV_STATE_DISABLED);
  }
  if (ui_AudioStopBtn) {
    lv_obj_clear_flag(ui_AudioStopBtn, LV_OBJ_FLAG_HIDDEN);
  }
}

void AudioStopBtn_cb(lv_event_t *e) {
  AudioManager::getInstance().stop();
  if (ui_AudioPlayBtn) {
    lv_obj_clear_state(ui_AudioPlayBtn, LV_STATE_DISABLED);
  }
  if (ui_AudioStopBtn) {
    lv_obj_add_flag(ui_AudioStopBtn, LV_OBJ_FLAG_HIDDEN);
  }
}

void VolumeUp_cb(lv_event_t *e) {
  uint8_t vol = AudioManager::getInstance().getVolume();
  if (vol < 21) {
    AudioManager::getInstance().setVolume(vol + 1);
    // Guardar en EEPROM de forma diferida (sin bloquear el bus flash)
    EEPROM.write(EEPROM_AUDIO_VOLUME, AudioManager::getInstance().getVolume());
    eepromDirty = true;
    lastVarChangeTime = millis();
  }
  if (ui_VolumeLabel) {
    char buf[12];
    snprintf(buf, sizeof(buf), "Vol: %d",
             AudioManager::getInstance().getVolume());
    lv_label_set_text(ui_VolumeLabel, buf);
  }
}

void VolumeDown_cb(lv_event_t *e) {
  uint8_t vol = AudioManager::getInstance().getVolume();
  if (vol > 1) {
    AudioManager::getInstance().setVolume(vol - 1);
    // Guardar en EEPROM de forma diferida (sin bloquear el bus flash)
    EEPROM.write(EEPROM_AUDIO_VOLUME, AudioManager::getInstance().getVolume());
    eepromDirty = true;
    lastVarChangeTime = millis();
  }
  if (ui_VolumeLabel) {
    char buf[12];
    snprintf(buf, sizeof(buf), "Vol: %d",
             AudioManager::getInstance().getVolume());
    lv_label_set_text(ui_VolumeLabel, buf);
  }
}

// ==========================================
// Main Task
// ==========================================
void UI_Task(void *pvParameters) {
  ESP_LOGI(TAG, "UI Task Started");

  // CrowPanel STC8H1K28 init + Backlight (I2C 0x30)
  {
    vTaskDelay(pdMS_TO_TICKS(100));

    // STC8 requires these commands before it accepts the backlight command
    uint8_t init_cmds[] = {247, 248}; // Buzzer OFF, Speaker ON
    for (uint8_t cmd : init_cmds) {
      Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
      Wire.write(cmd);
      Wire.endTransmission();
      vTaskDelay(pdMS_TO_TICKS(20));
    }

    // Según doc v1.3: 0 es Brillo Máximo, 245 es Apagado.
    Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
    Wire.write(DISPLAY_BL_ON_VALUE);
    Wire.endTransmission();

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Display initialization
  lcd.begin();

  // Clear screen to black initially
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextSize(2);
  // vTaskDelay(pdMS_TO_TICKS(DELAY_SHORT_MS)); // Skip delay

  // DIAGNOSTIC REMOVED: Direct boot to UI

  lv_init();

  // Try to initialize Touch
  bool touch_ok = false;
  for (int i = 0; i < 3; i++) {
    // Note: PCA9557 at 0x18 was not found in scan.
    // Touch reset is likely handled by STC8 (0x30) or already high.
    if (ts.begin()) {
      touch_ok = true;
      ESP_LOGI(TAG, "Touch controller initialized OK");
      break;
    }
    ESP_LOGW(TAG, "Touch init failed, retrying...");
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  if (!touch_ok) {
    ESP_LOGE(TAG, "Touch controller FAILED to init after retries. Touch may "
                  "trigger ghost clicks or not work.");
  }

  // ts.reset();
  ts.setRotation(TOUCH_ROTATION);
  lcd.setRotation(LCD_ROTATION);

  screenWidth = lcd.width();
  screenHeight = lcd.height();
  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL,
                        screenWidth * screenHeight / COLOR_DIVISOR);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  indev_drv.long_press_time = LOCK_PROGRESS_DURATION_MS;
  lv_indev_drv_register(&indev_drv);

  ui_init();
  g_ui_initialized = true;
  Communication_UIReady(); // Sincronización robusta: avisar a la Board que ya
                           // podemos pintar alarmas

  /* Comentado para v1.3 (Control vía I2C)
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(TFT_BL_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, BRIGHTNESS_MAX);
  */

  UI_ApplyLanguage(g_lang);
  ui_set_switch_state_silent(ui_SwitchDarkMode, darkMode);
  // UI_ApplyTheme() movida al final de la creación de elementos manuales para
  // que les afecte

  // Botón de PLAY de Audio (oculto — acceso de audio deshabilitado)
  ui_AudioPlayBtn = lv_btn_create(ui_ScreenSettings);
  lv_obj_set_size(ui_AudioPlayBtn, 120, 50);
  lv_obj_align(ui_AudioPlayBtn, LV_ALIGN_BOTTOM_RIGHT, -160, -20);
  ui_AudioPlayLabel = lv_label_create(ui_AudioPlayBtn);
  lv_label_set_text(ui_AudioPlayLabel, "Play Audio");
  lv_obj_center(ui_AudioPlayLabel);
  lv_obj_add_event_cb(ui_AudioPlayBtn, AudioTestBtn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(ui_AudioPlayBtn, LV_OBJ_FLAG_HIDDEN); // Oculto permanente
  lv_obj_clear_flag(ui_AudioPlayBtn, LV_OBJ_FLAG_CLICKABLE); // No interactuable

  // Botón de STOP de Audio (oculto — acceso de audio deshabilitado)
  ui_AudioStopBtn = lv_btn_create(ui_ScreenSettings);
  lv_obj_set_size(ui_AudioStopBtn, 120, 50);
  lv_obj_align(ui_AudioStopBtn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
  lv_obj_set_style_bg_color(ui_AudioStopBtn, lv_color_hex(0xFF0000),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_t *audioStopLabel = lv_label_create(ui_AudioStopBtn);
  lv_label_set_text(audioStopLabel, "Stop");
  lv_obj_center(audioStopLabel);
  lv_obj_add_event_cb(ui_AudioStopBtn, AudioStopBtn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(ui_AudioStopBtn, LV_OBJ_FLAG_HIDDEN); // Oculto permanente
  lv_obj_clear_flag(ui_AudioStopBtn, LV_OBJ_FLAG_CLICKABLE); // No interactuable

  // --- Fila de Volumen (oculta — acceso de audio deshabilitado) ---
  // Botón Vol-
  ui_VolumeDownBtn = lv_btn_create(ui_ScreenSettings);
  lv_obj_set_size(ui_VolumeDownBtn, 50, 40);
  lv_obj_align(ui_VolumeDownBtn, LV_ALIGN_BOTTOM_RIGHT, -260, -80);
  lv_obj_t *volDownLabel = lv_label_create(ui_VolumeDownBtn);
  lv_label_set_text(volDownLabel, LV_SYMBOL_MINUS);
  lv_obj_center(volDownLabel);
  lv_obj_add_event_cb(ui_VolumeDownBtn, VolumeDown_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(ui_VolumeDownBtn, LV_OBJ_FLAG_HIDDEN); // Oculto permanente
  lv_obj_clear_flag(ui_VolumeDownBtn, LV_OBJ_FLAG_CLICKABLE);

  // Label de volumen central
  ui_VolumeLabel = lv_label_create(ui_ScreenSettings);
  {
    char buf[12];
    snprintf(buf, sizeof(buf), "Vol: %d",
             AudioManager::getInstance().getVolume());
    lv_label_set_text(ui_VolumeLabel, buf);
  }
  lv_obj_set_style_text_font(ui_VolumeLabel, &lv_font_montserrat_16, 0);
  lv_obj_align(ui_VolumeLabel, LV_ALIGN_BOTTOM_RIGHT, -170, -90);
  lv_obj_add_flag(ui_VolumeLabel, LV_OBJ_FLAG_HIDDEN); // Oculto permanente

  // Botón Vol+
  ui_VolumeUpBtn = lv_btn_create(ui_ScreenSettings);
  lv_obj_set_size(ui_VolumeUpBtn, 50, 40);
  lv_obj_align(ui_VolumeUpBtn, LV_ALIGN_BOTTOM_RIGHT, -30, -80);
  lv_obj_t *volUpLabel = lv_label_create(ui_VolumeUpBtn);
  lv_label_set_text(volUpLabel, LV_SYMBOL_PLUS);
  lv_obj_center(volUpLabel);
  lv_obj_add_event_cb(ui_VolumeUpBtn, VolumeUp_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(ui_VolumeUpBtn, LV_OBJ_FLAG_HIDDEN); // Oculto permanente
  lv_obj_clear_flag(ui_VolumeUpBtn, LV_OBJ_FLAG_CLICKABLE);

  // Aplicar tema DESPUÉS de crear todos los elementos manuales (Play, Stop,
  // Volumen, etc.)
  UI_ApplyTheme();

  // --- Skin probe toast (RF-SKIN-010, UI-SKIN-004): mensaje de bloqueo al
  // activar modo piel sin sonda ---
  ui_SkinProbeToast = lv_label_create(lv_scr_act());
  lv_label_set_text(ui_SkinProbeToast, "");
  lv_label_set_long_mode(ui_SkinProbeToast, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(ui_SkinProbeToast, 320);
  lv_obj_align(ui_SkinProbeToast, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_set_style_text_align(ui_SkinProbeToast, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(ui_SkinProbeToast, lv_color_hex(0xFF8C00),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_SkinProbeToast, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_text_color(ui_SkinProbeToast, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN);
  lv_obj_set_style_pad_all(ui_SkinProbeToast, 10, LV_PART_MAIN);
  lv_obj_set_style_radius(ui_SkinProbeToast, 8, LV_PART_MAIN);
  lv_obj_add_flag(ui_SkinProbeToast, LV_OBJ_FLAG_HIDDEN);

  // --- Skin probe status label (RF-SKIN-004, UI-SKIN-005): informativo en modo
  // aire ---
  ui_SkinProbeStatusLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(ui_SkinProbeStatusLabel, "");
  lv_obj_align(ui_SkinProbeStatusLabel, LV_ALIGN_BOTTOM_LEFT, 10, -5);
  lv_obj_set_style_text_font(ui_SkinProbeStatusLabel, &lv_font_montserrat_12,
                             0);
  lv_obj_set_style_text_color(ui_SkinProbeStatusLabel, lv_color_hex(0x888888),
                              LV_PART_MAIN);
  lv_obj_add_flag(ui_SkinProbeStatusLabel, LV_OBJ_FLAG_HIDDEN);

  // --- AUTO AIR button & popup (UI-AUTOAIR-001..010) ---
  create_autoair_button();
  create_autoair_popup();
  autoair_update_button_style(); // set initial visual state after creation

  intro_timer = lv_timer_create(intro_timer_cb, 5000, NULL);
  lv_timer_set_repeat_count(intro_timer, 1);

  // Visuals
  lv_bar_set_range(ui_AirTempBar, 0, 20);
  lv_bar_set_range(ui_SkinTempBar, 0, 20);
  lv_bar_set_range(ui_HumBar, 0, 100);

  lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(ui_Keyboard1, NULL);

  lv_textarea_set_text(ui_TextArea2, wifi_pass);

  lv_color_t init_panel_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_WHITE;
  lv_color_t init_inactive_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_GRAY;

  lv_obj_set_style_bg_color(ui_Panel2, init_panel_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel2, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel5, init_panel_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel5, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel6, init_panel_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel6, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel4, init_panel_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel4, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_Panel1, init_inactive_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel3, init_inactive_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_ArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_ArrowUpTemp, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Alarm1Label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm1Panel, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Alarm2Label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm2Panel, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Alarm3Label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm3Panel, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Alarm4Panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm4Label, LV_OBJ_FLAG_CLICKABLE);

  // lv_obj_add_flag(ui_ChartButton, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SPO2Button, LV_OBJ_FLAG_HIDDEN);

  lv_obj_set_width(ui_AlarmDetailLabel, lv_pct(100));
  lv_label_set_long_mode(ui_AlarmDetailLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(ui_AlarmDetailLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(ui_AlarmDetailLabel, LV_ALIGN_TOP_MID, 0, 20);

  lv_label_set_text(ui_AlarmDetailLabel, "");

  lv_obj_add_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_AlarmButton, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowHumLock, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Spinner1, LV_OBJ_FLAG_HIDDEN);

  add_unlock_press_cb_recursive(ui_UnlockCont);

  lv_obj_add_flag(ui_AlarmLockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_CheckImg, LV_OBJ_FLAG_HIDDEN);

  airTempSeries =
      configure_temp_chart(ui_AirTempChart, LV_PALETTE_BLUE, 20, 40);
  skinTempSeries =
      configure_temp_chart(ui_SkinTempChart, LV_PALETTE_BLUE, 20, 40);

  lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TabView1, LV_OBJ_FLAG_HIDDEN);

  // Clean Hum chart series
  lv_chart_series_t *s = lv_chart_get_series_next(ui_HumChart, NULL);
  while (s != NULL) {
    lv_chart_series_t *next = lv_chart_get_series_next(ui_HumChart, s);
    s->x_points = NULL;
    s->x_ext_buf_assigned = 0;
    lv_chart_remove_series(ui_HumChart, s);
    s = next;
  }
  lv_chart_set_type(ui_HumChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(ui_HumChart, 50);
  lv_chart_set_range(ui_HumChart, LV_CHART_AXIS_PRIMARY_Y, 10, 100);
  humSeries = lv_chart_add_series(
      ui_HumChart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_axis_tick(ui_HumChart, LV_CHART_AXIS_SECONDARY_Y, 0, 0, 0, 0,
                         false, 0);
  lv_chart_set_axis_tick(ui_HumChart, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0,
                         false, 0);
  for (int i = 0; i < lv_chart_get_point_count(ui_HumChart); i++) {
    humSeries->y_points[i] = LV_CHART_POINT_NONE;
  }
  lv_chart_refresh(ui_HumChart);

  lv_timer_handler();

  update_labels();

  lv_obj_add_flag(ui_Label9, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Label15, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Label13, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Label16, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Label10, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Label17, LV_OBJ_FLAG_CLICKABLE);

  setup_panel_callbacks();
  setup_arrow_callbacks();
  setup_arrow_hum_callbacks();

  // Assign callbacks for Heater Error interaction
  if (ui_HeaterErrorTempCont)
    lv_obj_add_event_cb(ui_HeaterErrorTempCont, HeaterError_event_handler,
                        LV_EVENT_CLICKED, NULL);
  if (ui_HeaterErrorHumCont)
    lv_obj_add_event_cb(ui_HeaterErrorHumCont, HeaterError_event_handler,
                        LV_EVENT_CLICKED, NULL);
  if (ui_HeaterErrorTempLabel)
    lv_obj_add_event_cb(ui_HeaterErrorTempLabel, HeaterError_event_handler,
                        LV_EVENT_CLICKED, NULL);
  if (ui_HeaterErrorHumLabel)
    lv_obj_add_event_cb(ui_HeaterErrorHumLabel, HeaterError_event_handler,
                        LV_EVENT_CLICKED, NULL);

  lv_timer_create(inactivity_timer_cb, 1000, NULL);

  for (;;) {
    lv_timer_handler();

    // Gestión de estado de botones de Audio — deshabilitada (UI oculta
    // permanentemente) Si se quiere reactivar, descomentar el bloque siguiente
    // y quitar LV_OBJ_FLAG_HIDDEN de los widgets:
    /*
    if(ui_AudioPlayBtn) {
        if(AudioManager::getInstance().isPlaying()) {
            if(!lv_obj_has_state(ui_AudioPlayBtn, LV_STATE_DISABLED)) {
                lv_obj_add_state(ui_AudioPlayBtn, LV_STATE_DISABLED);
            }
            if(ui_AudioStopBtn && lv_obj_has_flag(ui_AudioStopBtn,
    LV_OBJ_FLAG_HIDDEN)) { lv_obj_clear_flag(ui_AudioStopBtn,
    LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            if(lv_obj_has_state(ui_AudioPlayBtn, LV_STATE_DISABLED)) {
                lv_obj_clear_state(ui_AudioPlayBtn, LV_STATE_DISABLED);
            }
            if(ui_AudioStopBtn && !lv_obj_has_flag(ui_AudioStopBtn,
    LV_OBJ_FLAG_HIDDEN)) { lv_obj_add_flag(ui_AudioStopBtn, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    */

    // AudioManager::getInstance().loop(); // Ahora corre en su propia tarea
    // (Core 0)

    // Debug Pulse
    static uint32_t lastLoopMs = 0;
    static uint32_t lastSyncMs = 0;

    if (millis() - lastSyncMs > 1000) {
      lastSyncMs = millis();
      UI_SyncAll();
    }

    if (millis() - lastLoopMs > 5000) {
      lastLoopMs = millis();
      ESP_LOGD(TAG, "UI and Audio Loop active");
    }
    vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));

    if (pendingReconnect && (millis() - disconnectTimestampMs >= 5000)) {
      pendingReconnect = false;
      wifiInit();
    }

    if (wifiVisible) {
      lv_obj_clear_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
      bool actuallyConnected = (WiFi.status() == WL_CONNECTED);
      if (actuallyConnected != isConnected) {
        isConnected = actuallyConnected;
        updateButtonVisibility();
      }
      if (isConnected) {
        lv_obj_clear_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_WifiSSIDLabel, WiFi.SSID().c_str());
      } else {
        lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (photoTimerActive) {
      unsigned long elapsed = millis() - photoTimerStartMs;
      long totalSeconds = photoTimerMinutes * 60;
      long remaining = totalSeconds - (elapsed / 1000);

      if (remaining <= 0) {
        // Timer finished
        photoTimerActive = false;
        hmi_msg.phototherapyMode = PHOTOTHERAPY_OFF;
        hmi_msg.shouldSendData = true;

        // Turn switch OFF visually and trigger callback
        lv_obj_clear_state(ui_Switch3, LV_STATE_CHECKED);
        lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);

        if (ui_PhotoCancelBtn)
          lv_obj_add_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);
      } else {
        // Update countdown display
        int totalMins =
            (remaining + 59) / 60; // Round up to show minutes correctly
        int hours = totalMins / 60;
        int mins = totalMins % 60;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d", hours, mins);

        // Update main screen
        lv_label_set_text(ui_PhotoTimeValueLabel, buf);

        // Update lock screen
        if (ui_PhotoLockTimeLabel) {
          lv_label_set_text(ui_PhotoLockTimeLabel, buf);
        }
        if (ui_PhotoLockCont) {
          lv_obj_clear_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
        }
      }
    } else if (hmi_msg.phototherapyMode == PHOTOTHERAPY_ON) {
      // Phototherapy is ON but no timer is active
      if (ui_PhotoLockTimeLabel) {
        // Use translation for "ON" if available, or just "ON"
        lv_label_set_text(ui_PhotoLockTimeLabel, "ON");
      }
      if (ui_PhotoLockCont) {
        lv_obj_clear_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      lastPhotoMinutesSent = -1;
      // Timer not active and light is OFF, hide lock screen container
      if (ui_PhotoLockCont) {
        lv_obj_add_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (g_pendingAlarmUpdate) {
      update_alarm_panels();
      g_pendingAlarmUpdate = false;
    }

    if (eepromDirty && (millis() - lastVarChangeTime > EEPROM_COMMIT_DELAY)) {
      EEPROM.commit();
      eepromDirty = false;
      ESP_LOGI(TAG, "EEPROM committed after delay");
    }
  }
}

void CreateUITask() {
  xTaskCreatePinnedToCore(UI_Task, "UI", 8192 * 2, NULL, 2, NULL,
                          CORE_ID_FREERTOS);
}

void UI_SyncAll() {
  if (!g_ui_initialized)
    return;

  lv_color_t active_col = darkMode ? COLOR_PANEL_GRAY : COLOR_PANEL_WHITE;
  lv_color_t inactive_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_GRAY;

  // 1. Update internal state flags from current switch visual state
  switchTemp = lv_obj_has_state(ui_Switch1, LV_STATE_CHECKED);
  tempSwitched = switchTemp;
  switchHum = lv_obj_has_state(ui_Switch2, LV_STATE_CHECKED);
  humSwitched = switchHum;
  skinPanelEnabled = lv_obj_has_state(ui_Switch4, LV_STATE_CHECKED);

  // RF-SKIN-004/RF-SKIN-008: Update skin probe informative status label
  // (UI-SKIN-002/005)
  if (ui_SkinProbeStatusLabel) {
    bool inSkinMode = skinPanelEnabled && (hmi_msg.controlMode == CONTROL_SKIN);
    if (!inSkinMode && g_skinProbeState != SKIN_PROBE_VALID) {
      // Show informative (non-critical) probe status in air mode
      const char *statusTxt = nullptr;
      switch (g_skinProbeState) {
      case SKIN_PROBE_NOT_CONNECTED:
        statusTxt = (g_lang == LANG_ES)   ? "Sonda piel: no conectada"
                    : (g_lang == LANG_FR) ? "Sonde peau: non connectee"
                                          : "Skin probe: not connected";
        break;
      case SKIN_PROBE_PENDING_VALIDATION:
        statusTxt = (g_lang == LANG_ES)   ? "Sonda piel: validando..."
                    : (g_lang == LANG_FR) ? "Sonde peau: validation..."
                                          : "Skin probe: validating...";
        break;
      case SKIN_PROBE_UNSTABLE:
        statusTxt = (g_lang == LANG_ES)   ? "Sonda piel: señal inestable"
                    : (g_lang == LANG_FR) ? "Sonde peau: signal instable"
                                          : "Skin probe: unstable signal";
        break;
      default:
        statusTxt = nullptr;
        break;
      }
      if (statusTxt) {
        lv_label_set_text(ui_SkinProbeStatusLabel, statusTxt);
        lv_obj_clear_flag(ui_SkinProbeStatusLabel, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(ui_SkinProbeStatusLabel, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      lv_obj_add_flag(ui_SkinProbeStatusLabel, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // 2. Temperature Logic
  if (tempSwitched) {
    if (selectedPanel == NO_PANEL_SELECTED) {
      selectedPanel = (lastSelectedPanel != NO_PANEL_SELECTED)
                          ? lastSelectedPanel
                          : AIR_PANEL_SELECTED;
    }

    if (selectedPanel == AIR_PANEL_SELECTED) {
      set_active_panel(ui_AirPanel, ui_SkinPanel);
      lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);

      // Target Screen Visibility
      if (ui_TargetAirTempCont)
        lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowAirLock)
        lv_obj_clear_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
      if (ui_TargetSkinTempCont)
        lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowSkinLock)
        lv_obj_add_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);

      hmi_msg.controlMode = CONTROL_AIR;
    } else {
      set_active_panel(ui_SkinPanel, ui_AirPanel);
      lv_obj_clear_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);

      // Target Screen Visibility
      if (ui_TargetSkinTempCont)
        lv_obj_clear_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowSkinLock)
        lv_obj_clear_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);
      if (ui_TargetAirTempCont)
        lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowAirLock)
        lv_obj_add_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);

      hmi_msg.controlMode = CONTROL_SKIN;
    }

    // First Column (Detected Values) ALWAYS VISIBLE
    if (ui_AirTempLockCont)
      lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_SkinTempLockCont)
      lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_HumLockCont)
      lv_obj_clear_flag(ui_HumLockCont, LV_OBJ_FLAG_HIDDEN);

    // Enable arrows
    arrowsActive = true;
    lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownTemp, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpTemp, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowUpTemp, LV_OPA_COVER, LV_PART_MAIN);

    // Temperature Panel background
    lv_obj_set_style_bg_color(ui_Panel1, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);

    temp_chart_show_for_selected_panel();
  } else {
    selectedPanel = NO_PANEL_SELECTED;
    arrowsActive = false;
    lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownTemp, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpTemp, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowUpTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Panel1, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_AirPanel, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_SkinPanel, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

    // Visually show labels and thermometer of the last selected panel even if
    // OFF
    if (lastSelectedPanel == SKIN_PANEL_SELECTED) {
      lv_obj_clear_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
    }

    // Lock Screen hide targets but keep detected values visible
    if (ui_TargetAirTempCont)
      lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_TargetSkinTempCont)
      lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_ArrowAirLock)
      lv_obj_add_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
    if (ui_ArrowSkinLock)
      lv_obj_add_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);

    if (ui_AirTempLockCont)
      lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_SkinTempLockCont)
      lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_HumLockCont)
      lv_obj_clear_flag(ui_HumLockCont, LV_OBJ_FLAG_HIDDEN);
  }

  // 3. Humidity Logic
  if (switchHum) {
    lv_obj_clear_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownHum, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpHum, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Panel3, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);

    // Apply active color to the inner humidity panel
    if (ui_HumPanelCont) {
      lv_obj_set_style_bg_color(ui_HumPanelCont, active_col, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_HumPanelCont, LV_OPA_COVER, LV_PART_MAIN);
    }
  } else {
    lv_obj_add_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownHum, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpHum, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Panel3, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);

    // Apply inactive color to the inner humidity panel
    if (ui_HumPanelCont) {
      lv_obj_set_style_bg_color(ui_HumPanelCont, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_HumPanelCont, LV_OPA_COVER, LV_PART_MAIN);
    }
  }

  // 4. Phototherapy Logic (Switch 3)
  bool photoOn = lv_obj_has_state(ui_Switch3, LV_STATE_CHECKED);

  if (photoOn) {
    // Normal state
    lv_obj_set_style_bg_color(ui_PhotoTimerPanel, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_PhotoTimerPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(ui_PhotoTimeMinusBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_PhotoTimePlusBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_PhotoStartBtn, LV_OBJ_FLAG_CLICKABLE);

    // Restore button colors
    lv_obj_set_style_bg_color(ui_PhotoTimeMinusBtn, lv_color_hex(0x0075EE),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_PhotoTimePlusBtn, lv_color_hex(0x0075EE),
                              LV_PART_MAIN);

    if (photoTimerActive) {
      // Update visual running state
      const char *TXT_RUNNING[] = {"EJECUTANDO", "RUNNING", "EN COURS"};
      if (ui_PhotoStartLabel)
        lv_label_set_text(ui_PhotoStartLabel, TXT_RUNNING[g_lang]);
      if (ui_PhotoStartBtn)
        lv_obj_set_style_bg_color(ui_PhotoStartBtn, lv_color_hex(0x888888),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

      // Ensure cancel button is visible
      if (ui_PhotoCancelBtn)
        lv_obj_clear_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);

      // Ensure lock screen is visible if we are in lock screen
      if (ui_PhotoLockCont)
        lv_obj_clear_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
    } else {
      // Setup state
      const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
      if (ui_PhotoStartLabel)
        lv_label_set_text(ui_PhotoStartLabel, TXT_PHOTOSTART[g_lang]);
      if (ui_PhotoStartBtn)
        lv_obj_set_style_bg_color(ui_PhotoStartBtn, lv_color_hex(0x00AA00),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

      // Ensure cancel button is hidden
      if (ui_PhotoCancelBtn)
        lv_obj_add_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);

      if (ui_PhotoLockCont)
        lv_obj_add_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);

      // Restore normal minutes display if not active
      char buf[16];
      snprintf(buf, sizeof(buf), "%d min", photoTimerMinutes);
      if (ui_PhotoTimeValueLabel)
        lv_label_set_text(ui_PhotoTimeValueLabel, buf);
    }
  } else {
    // Grayed out state
    lv_obj_set_style_bg_color(ui_PhotoTimerPanel, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_PhotoTimerPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(ui_PhotoTimeMinusBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_PhotoTimePlusBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_PhotoStartBtn, LV_OBJ_FLAG_CLICKABLE);

    // Buttons to light gray as requested
    lv_obj_set_style_bg_color(ui_PhotoTimeMinusBtn, COLOR_PANEL_LIGHT_GRAY,
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_PhotoTimePlusBtn, COLOR_PANEL_LIGHT_GRAY,
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_PhotoStartBtn, COLOR_PANEL_LIGHT_GRAY,
                              LV_PART_MAIN);

    // Reset visual values
    if (!photoTimerActive) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%d min", photoTimerMinutes);
      if (ui_PhotoTimeValueLabel)
        lv_label_set_text(ui_PhotoTimeValueLabel, buf);

      const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
      if (ui_PhotoStartLabel)
        lv_label_set_text(ui_PhotoStartLabel, TXT_PHOTOSTART[g_lang]);
    }

    if (ui_PhotoCancelBtn)
      lv_obj_add_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);
    if (ui_PhotoLockCont)
      lv_obj_add_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
    photoTimerActive = false; // Safety
  }

  // Panel 2 is now handled above based on photoOn state
  // lv_obj_set_style_bg_color(ui_Panel2, active_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel2, LV_OPA_COVER, LV_PART_MAIN);

  // 5. Skin Block (Switch 4)
  if (skinPanelEnabled) {
    lv_obj_clear_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
  }

  // 6. TabView Management
  lv_obj_t *tab_btns_cont = lv_obj_get_child(ui_TabView1, 0);
  lv_obj_t *temp_tab_btn = NULL;
  lv_obj_t *hum_tab_btn = NULL;
  if (tab_btns_cont) {
    temp_tab_btn = lv_obj_get_child(tab_btns_cont, 0);
    hum_tab_btn = lv_obj_get_child(tab_btns_cont, 1);
  }

  if (!switchTemp && !switchHum) {
    lv_obj_add_flag(ui_TabView1, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(ui_TabView1, LV_OBJ_FLAG_HIDDEN);
    if (temp_tab_btn) {
      if (switchTemp)
        lv_obj_clear_flag(temp_tab_btn, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(temp_tab_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (hum_tab_btn) {
      if (switchHum)
        lv_obj_clear_flag(hum_tab_btn, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(hum_tab_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if ((switchTemp && !switchHum) || (!switchTemp && switchHum)) {
      if (tab_btns_cont)
        lv_obj_add_flag(tab_btns_cont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(
          lv_tabview_get_content(ui_TabView1),
          LV_OBJ_FLAG_SCROLLABLE); // Bloquear swipe si solo hay uno
    } else {
      if (tab_btns_cont)
        lv_obj_clear_flag(tab_btns_cont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_tabview_get_content(ui_TabView1),
                      LV_OBJ_FLAG_SCROLLABLE); // Permitir swipe si hay dos
    }

    if (switchTemp && !switchHum)
      lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_OFF);
    else if (!switchTemp && switchHum)
      lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_OFF);

    // --- Sincronización de la pantalla de Historial ---
    // Gráfica de Temperatura: Aire o Piel según el modo activo
    if (switchTemp) {
      if (selectedPanel == SKIN_PANEL_SELECTED) {
        lv_obj_add_flag(ui_HistoryChartAire, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_HistoryChartAireLabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_HistoryValueAire, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_HistoryChartSkin, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_HistoryChartSkinLabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_HistoryValueSkin, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(ui_HistoryChartAire, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_HistoryChartAireLabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_HistoryValueAire, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_HistoryChartSkin, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_HistoryChartSkinLabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_HistoryValueSkin, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      // Ambas ocultas si temperatura está OFF
      lv_obj_add_flag(ui_HistoryChartAire, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_HistoryChartAireLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_HistoryValueAire, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_HistoryChartSkin, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_HistoryChartSkinLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_HistoryValueSkin, LV_OBJ_FLAG_HIDDEN);
    }

    // Gráfica de Humedad
    if (switchHum) {
      lv_obj_clear_flag(ui_HistoryChartHum, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_HistoryChartHumLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_HistoryValueHum, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ui_HistoryChartHum, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_HistoryChartHumLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_HistoryValueHum, LV_OBJ_FLAG_HIDDEN);
    }
  }

  hmi_msg.language = (int)g_lang;
  update_labels();
  autoair_update_button_style(); // ARQ-AUTOAIR-005: keep button style in sync
}

static void UI_ApplyStyleToLabelsRecursive(lv_obj_t *obj, lv_color_t color) {
  if (!obj)
    return;

  // Check if the object is a label.
  // Note: In some LVGL versions we use lv_obj_get_class(obj) == &lv_label_class
  // or lv_obj_check_type(obj, &lv_label_class).
  if (lv_obj_check_type(obj, &lv_label_class)) {
    lv_obj_set_style_text_color(obj, color, 0);
  }

  uint32_t i;
  uint32_t n = lv_obj_get_child_cnt(obj);
  for (i = 0; i < n; i++) {
    UI_ApplyStyleToLabelsRecursive(lv_obj_get_child(obj, i), color);
  }
}

void UI_ApplyTheme() {
  if (!g_ui_initialized)
    return;

  lv_color_t bg_col = darkMode
                          ? COLOR_BG_DARK
                          : lv_color_hex(0xF4F4F4); // Original light gray bg
  lv_color_t panel_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_WHITE;
  lv_color_t text_col = darkMode ? COLOR_TEXT_DARK : lv_color_hex(0x000000);

  // Apply to all screens background
  if (ui_ScreenMain)
    lv_obj_set_style_bg_color(ui_ScreenMain, bg_col, 0);
  if (ui_ScreenSettings)
    lv_obj_set_style_bg_color(ui_ScreenSettings, bg_col, 0);
  if (ui_ScreenAlarms)
    lv_obj_set_style_bg_color(ui_ScreenAlarms, bg_col, 0);
  if (ui_ScreenCharts)
    lv_obj_set_style_bg_color(ui_ScreenCharts, bg_col, 0);
  if (ui_ScreenPulseOxi)
    lv_obj_set_style_bg_color(ui_ScreenPulseOxi, bg_col, 0);

  // Main Screen Panels
  if (ui_Panel1)
    lv_obj_set_style_bg_color(ui_Panel1, panel_col, 0);
  if (ui_Panel2)
    lv_obj_set_style_bg_color(ui_Panel2, panel_col, 0);
  if (ui_Panel3)
    lv_obj_set_style_bg_color(ui_Panel3, panel_col, 0);
  if (ui_Panel4)
    lv_obj_set_style_bg_color(ui_Panel4, panel_col, 0);
  if (ui_Panel5)
    lv_obj_set_style_bg_color(ui_Panel5, panel_col, 0);
  if (ui_Panel6)
    lv_obj_set_style_bg_color(ui_Panel6, panel_col, 0);

  // Settings Panels
  if (ui_Panel7)
    lv_obj_set_style_bg_color(ui_Panel7, panel_col, 0);
  if (ui_Panel8)
    lv_obj_set_style_bg_color(ui_Panel8, panel_col, 0);
  if (ui_Panel9)
    lv_obj_set_style_bg_color(ui_Panel9, panel_col, 0);
  if (ui_PanelDarkMode)
    lv_obj_set_style_bg_color(ui_PanelDarkMode, panel_col, 0);
  if (ui_InfoPanel)
    lv_obj_set_style_bg_color(ui_InfoPanel, panel_col, 0);

  // Apply text color to all labels in all screens
  if (ui_ScreenMain)
    UI_ApplyStyleToLabelsRecursive(ui_ScreenMain, text_col);
  if (ui_ScreenSettings)
    UI_ApplyStyleToLabelsRecursive(ui_ScreenSettings, text_col);
  if (ui_ScreenAlarms)
    UI_ApplyStyleToLabelsRecursive(ui_ScreenAlarms, text_col);
  if (ui_ScreenCharts)
    UI_ApplyStyleToLabelsRecursive(ui_ScreenCharts, text_col);
  if (ui_ScreenPulseOxi)
    UI_ApplyStyleToLabelsRecursive(ui_ScreenPulseOxi, text_col);
  // Lock Screen ALWAYS dark with white text
  if (ui_ScreenLock) {
    lv_obj_set_style_bg_color(ui_ScreenLock, lv_color_hex(0x1A1A1A), 0);
    UI_ApplyStyleToLabelsRecursive(ui_ScreenLock, lv_color_hex(0xFFFFFF));
  }

  // Settings Details (Info/Wifi) ALWAYS keep black text because their panels
  // are white
  if (ui_InfoDetailsCont)
    UI_ApplyStyleToLabelsRecursive(ui_InfoDetailsCont, lv_color_hex(0x000000));
  if (ui_WifiConfigCont)
    UI_ApplyStyleToLabelsRecursive(ui_WifiConfigCont, lv_color_hex(0x000000));
  if (ui_WifiConnectedCont)
    UI_ApplyStyleToLabelsRecursive(ui_WifiConnectedCont,
                                   lv_color_hex(0x000000));

  // Images recoloring for Dark Mode (Originals are black, we want white)
  lv_color_t img_recolor =
      darkMode ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
  lv_opa_t img_recolor_opa = darkMode ? LV_OPA_COVER : LV_OPA_TRANSP;

  if (ui_ChartButton) {
    lv_obj_set_style_img_recolor(ui_ChartButton, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_ChartButton, img_recolor_opa, 0);
  }
  if (ui_ImgButton2) {
    lv_obj_set_style_img_recolor(ui_ImgButton2, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_ImgButton2, img_recolor_opa, 0);
  }
  if (ui_ImgButton7) {
    lv_obj_set_style_img_recolor(ui_ImgButton7, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_ImgButton7, img_recolor_opa, 0);
  }
  if (ui_ImgButton8) {
    lv_obj_set_style_img_recolor(ui_ImgButton8, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_ImgButton8, img_recolor_opa, 0);
  }
  if (ui_ImgButton9) {
    lv_obj_set_style_img_recolor(ui_ImgButton9, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_ImgButton9, img_recolor_opa, 0);
  }
  if (ui_Image4) {
    lv_obj_set_style_img_recolor(ui_Image4, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_Image4, img_recolor_opa, 0);
  }

  // --- CHARTS DARK MODE APLICATION ---
  lv_color_t chart_bg_col =
      darkMode ? COLOR_PANEL_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t chart_text_col =
      darkMode ? COLOR_TEXT_DARK : lv_color_hex(0x808080);
  lv_color_t chart_grid_col =
      darkMode ? lv_color_hex(0x222222) : lv_color_hex(0xEEEEEE);

  lv_obj_t *charts[] = {
      ui_AirTempChart,     ui_SkinTempChart,   ui_HumChart, ui_HistoryChartAire,
      ui_HistoryChartSkin, ui_HistoryChartHum, ui_OxChart};

  for (int i = 0; i < 7; i++) {
    if (charts[i] != NULL) {
      // Fondo del chart
      lv_obj_set_style_bg_color(charts[i], chart_bg_col, LV_PART_MAIN);

      // Color de la malla / cuadrícula
      lv_obj_set_style_line_color(charts[i], chart_grid_col, LV_PART_MAIN);

      // Color del texto de los ejes
      lv_obj_set_style_text_color(charts[i], chart_text_col, LV_PART_TICKS);
    }
  }

  // --- TABVIEWS DARK MODE APLICATION ---
  lv_color_t tab_bg_col = darkMode ? COLOR_BG_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t tab_page_bg_col =
      darkMode ? COLOR_PANEL_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t tab_btn_bg_col =
      darkMode ? COLOR_PANEL_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t tab_text_col = darkMode ? COLOR_TEXT_DARK : lv_color_hex(0x000000);

  lv_obj_t *tabviews[] = {ui_AlarmsTabview, ui_TabViewMainCharts, ui_TabView1};

  for (int i = 0; i < 3; i++) {
    if (tabviews[i] != NULL) {
      // 1. Fondo Principal de la vista del Tab
      lv_obj_set_style_bg_color(tabviews[i], tab_bg_col, LV_PART_MAIN);

      // 2. Fondo de los Botones (Header)
      lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabviews[i]);
      if (tab_btns != NULL) {
        // El contenedor en sí
        lv_obj_set_style_bg_color(tab_btns, tab_btn_bg_col, LV_PART_MAIN);
        // Los items individuales (las paletas clickeables)
        lv_obj_set_style_bg_color(tab_btns, tab_btn_bg_col, LV_PART_ITEMS);
        lv_obj_set_style_text_color(tab_btns, tab_text_col, LV_PART_ITEMS);
      }

      // 3. Aplicar fondo a CADA página interna (LV_DIR_CONTENT sub-children)
      // LVGL estructura habitualmente tabview -> [0] tab_btns, [1]
      // tab_content_container -> tabs
      lv_obj_t *content_cont = lv_tabview_get_content(tabviews[i]);
      if (content_cont) {
        // El root donde están las páginas
        lv_obj_set_style_bg_color(content_cont, tab_page_bg_col, LV_PART_MAIN);

        uint32_t tab_cnt = lv_obj_get_child_cnt(content_cont);
        for (uint32_t t = 0; t < tab_cnt; t++) {
          lv_obj_t *real_page = lv_obj_get_child(content_cont, t);
          if (real_page) {
            lv_obj_set_style_bg_color(real_page, tab_page_bg_col, LV_PART_MAIN);
          }
        }
      }
    }
  }

  // --- DROPDOWNS DARK MODE APLICATION ---
  lv_color_t dd_bg_col = darkMode ? COLOR_PANEL_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t dd_text_col = darkMode ? COLOR_TEXT_DARK : lv_color_hex(0x000000);
  lv_color_t dd_list_bg = darkMode ? COLOR_PANEL_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t dd_list_sel =
      darkMode ? lv_color_hex(0x555555) : lv_palette_main(LV_PALETTE_BLUE);

  lv_obj_t *dropdowns[] = {ui_HistoryDropdown, ui_LanguagesDropDown};

  for (int i = 0; i < 2; i++) {
    if (dropdowns[i] != NULL) {
      // Estilo del botón principal del dropdown
      lv_obj_set_style_bg_color(dropdowns[i], dd_bg_col, LV_PART_MAIN);
      lv_obj_set_style_text_color(dropdowns[i], dd_text_col, LV_PART_MAIN);

      // Estilo de la lista que se despliega
      lv_obj_t *list = lv_dropdown_get_list(dropdowns[i]);
      if (list != NULL) {
        // Fondo de la lista al desplegar
        lv_obj_set_style_bg_color(list, dd_list_bg, LV_PART_MAIN);
        lv_obj_set_style_border_color(
            list, darkMode ? lv_color_hex(0x444444) : lv_color_hex(0xCCCCCC),
            LV_PART_MAIN);
        // Texto de las opciones
        lv_obj_set_style_text_color(list, dd_text_col, LV_PART_MAIN);

        // Color de la opción seleccionada
        lv_obj_set_style_bg_color(list, dd_list_sel, LV_PART_SELECTED);
      }
    }
  }

  // --- AUTO AIR POPUP DARK MODE ---
  if (ui_AutoAirModal) {
    lv_color_t aa_modal_bg  = darkMode ? COLOR_BG_DARK          : COLOR_PANEL_WHITE;
    lv_color_t aa_row_bg    = darkMode ? COLOR_PANEL_DARK        : COLOR_PANEL_WHITE;
    lv_color_t aa_row_brd   = darkMode ? lv_color_hex(0x555555)  : lv_color_hex(0xDDDDDD);
    lv_color_t aa_sep_col   = darkMode ? lv_color_hex(0x444444)  : lv_color_hex(0xDDDDDD);

    lv_obj_set_style_bg_color(ui_AutoAirModal, aa_modal_bg, LV_PART_MAIN);

    lv_obj_t *rows[] = { ui_AutoAirRowGest, ui_AutoAirRowDays, ui_AutoAirRowWeight };
    for (int i = 0; i < 3; i++) {
      if (rows[i]) {
        lv_obj_set_style_bg_color(rows[i],     aa_row_bg,  LV_PART_MAIN);
        lv_obj_set_style_border_color(rows[i], aa_row_brd, LV_PART_MAIN);
      }
    }
    if (ui_AutoAirHSep) lv_obj_set_style_bg_color(ui_AutoAirHSep, aa_sep_col, LV_PART_MAIN);
    if (ui_AutoAirVSep) lv_obj_set_style_bg_color(ui_AutoAirVSep, aa_sep_col, LV_PART_MAIN);

    // Re-apply blue to value labels overridden by the recursive text sweep
    lv_color_t blue = lv_color_hex(0x0075EE);
    if (ui_AutoAirGestVal)   lv_obj_set_style_text_color(ui_AutoAirGestVal,   blue, 0);
    if (ui_AutoAirDaysVal)   lv_obj_set_style_text_color(ui_AutoAirDaysVal,   blue, 0);
    if (ui_AutoAirWeightVal) lv_obj_set_style_text_color(ui_AutoAirWeightVal, blue, 0);
    if (aa_label_mid)        lv_obj_set_style_text_color(aa_label_mid,        blue, 0);

    // Range bar: background light, indicator gray in dark mode
    if (aa_range_bar) {
      lv_obj_set_style_bg_color(aa_range_bar,
          darkMode ? lv_color_hex(0xCCCCCC) : lv_color_hex(0xE0E0E0), LV_PART_MAIN);
      lv_obj_set_style_bg_color(aa_range_bar,
          darkMode ? lv_color_hex(0x5588AA) : lv_color_hex(0x0095DA), LV_PART_INDICATOR);
    }
  }

  UI_SyncAll();
}

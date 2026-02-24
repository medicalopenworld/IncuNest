#include "UITask.h"
#include "CommTask.h"
#include "buzzer.h"
#include "esp_log.h"
#include "main.h"
#include "ui.h"
#include <LovyanGFX.hpp>
#include <PCA9557.h>
#include <SPI.h>
#include <TAMC_GT911.h>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include "AudioManager.h"

static const char *TAG = "UI";
static lv_obj_t *ui_AudioPlayBtn;
static lv_obj_t *ui_AudioStopBtn;
static lv_obj_t *ui_AudioPlayLabel;

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

bool alarmActive = false;
bool alarmsMuted = false;

bool prevTempAlarm = false;
bool prevHumAlarm = false;
int alarmSlotToIndex[MAX_ALARM_DISPLAY] = {-1, -1, -1, -1};

int chartLastPressed = -1;

bool wifiVisible = false;
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
      
      // Pinout based on CrowPanel Advance 7.0 sources (Harald Kreuzer + Inference)
      // Green (G0-G5): 10, 11, 12, 13, 14, 14? (Harald had 5 pins? G0-G4? G5 is usually MSB)
      // Let's map G0..G5. Harald listed 10, 11, 12, 13, 14. That's 5 pins. RGB565 has 6 Green bits.
      // Maybe G0 is LSB (GPIO 10)? G5 is MSB.
      // We are missing one bit. Let's assume G0 is grounded or shared? Or Harald list incomplete.
      // Let's map what we have:
      // Official CrowPanel Advance 7.0 (S3) Pinout from User Source Code
      
      // Official CrowPanel Advance 7.0 (S3) Pinout from User Success Example
      
      // Blue (B0-B4)
      cfg.pin_d0 = GPIO_NUM_21;    // B0
      cfg.pin_d1 = GPIO_NUM_47;    // B1
      cfg.pin_d2 = GPIO_NUM_48;    // B2
      cfg.pin_d3 = GPIO_NUM_45;    // B3
      cfg.pin_d4 = GPIO_NUM_38;    // B4
      
      // Green (G0-G5)
      cfg.pin_d5 = GPIO_NUM_9;    // G0
      cfg.pin_d6 = GPIO_NUM_10;   // G1
      cfg.pin_d7 = GPIO_NUM_11;   // G2
      cfg.pin_d8 = GPIO_NUM_12;   // G3
      cfg.pin_d9 = GPIO_NUM_13;   // G4
      cfg.pin_d10 = GPIO_NUM_14;  // G5
      
      // Red (R0-R4)
      cfg.pin_d11 = GPIO_NUM_7;   // R0
      cfg.pin_d12 = GPIO_NUM_17;  // R1
      cfg.pin_d13 = GPIO_NUM_18;  // R2
      cfg.pin_d14 = GPIO_NUM_3;   // R3
      cfg.pin_d15 = GPIO_NUM_46;  // R4

      cfg.pin_henable = GPIO_NUM_42;
      cfg.pin_vsync = GPIO_NUM_41;
      cfg.pin_hsync = GPIO_NUM_40;
      cfg.pin_pclk = GPIO_NUM_39;
      cfg.freq_write = 15000000;
      
      cfg.hsync_polarity = 1;
      cfg.hsync_pulse_width = 4;
      cfg.hsync_back_porch = 8;
      cfg.hsync_front_porch = 8;
      
      cfg.vsync_polarity = 1;
      cfg.vsync_pulse_width = 4;
      cfg.vsync_back_porch = 8;
      cfg.vsync_front_porch = 8;

      cfg.pclk_idle_high = 1;
      // We must likely use a separate SPI bus to init, OR Bus_RGB supports it?
      // LGFX v1 Bus_RGB struct has no SPI pins.
      // However, we can manual init in constructor?
      
      _bus_instance.config(cfg);
    }
    {
      auto cfg = _panel_instance.config_detail();
      cfg.use_psram = 1;
      _panel_instance.config_detail(cfg);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width = 800;
      cfg.memory_height = 480;
      cfg.panel_width = 800;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      _panel_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);
  }

  // Manual SPI Bitbang for SC7277 (9-bit: 1 D/C bit + 8 data bits)
  // Manual SPI Bitbang for SC7277 (9-bit: 1 D/C bit + 8 data bits)
  void spi_write_9bit(uint16_t data) {
      // We are not sure if CS is 19 (HSPI_SS) or 20 (Common).
      // Strategy: Drive BOTH low to ensure the display picks it up if it's either.
      int CS_A = 19; 
      int CS_B = 20;
      int CLKid = 39; // PCLK (Updated)
      int MOSIid = 40; // HSYNC / SDA (Updated - typically HSYNC is used as MOSI in 3-wire SPI RGB)
      
      pinMode(CS_A, OUTPUT);
      pinMode(CS_B, OUTPUT);
      pinMode(CLKid, OUTPUT);
      pinMode(MOSIid, OUTPUT);
      
      digitalWrite(CS_A, LOW);
      digitalWrite(CS_B, LOW);
      
      for(int i=0; i<9; i++) {
          digitalWrite(CLKid, LOW);
          // Send MSB first. Data is 9 bits.
          digitalWrite(MOSIid, (data & (1<<(8-i))) ? HIGH : LOW);
          delayMicroseconds(1);
          digitalWrite(CLKid, HIGH);
          delayMicroseconds(1);
      }
      digitalWrite(CS_A, HIGH);
      digitalWrite(CS_B, HIGH);
  }

  void sc7277_init() {
      // SWRESET (0x01) - Command (0 bit) + 0x01
      spi_write_9bit(0x001); 
      delay(120);
      
      // SLPOUT (0x11)
      spi_write_9bit(0x011); 
      delay(120);
      
      // DISPON (0x29)
      spi_write_9bit(0x029); 
      delay(10);
  }
};

LGFX lcd;

// Update Touch Pins to 15, 16
TAMC_GT911 ts = TAMC_GT911(15, 16, TOUCH_INT_PIN,
                           TOUCH_RST_PIN, 800, 480);

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
#if (LV_COLOR_16_SWAP != 0)
  lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t *)&color_p->full);
#else
  lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t *)&color_p->full);
#endif
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
  char buffer[BUFFER_SIZE];
  snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValue);
  lv_label_set_text(ui_TempAirDesired, buffer);
  lv_label_set_text(ui_TargetAirTempNumLabel, buffer);
  snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValue);
  lv_label_set_text(ui_TempSkinDesired, buffer);
  lv_label_set_text(ui_TargetSkinTempNumLabel, buffer);
  snprintf(buffer, sizeof(buffer), "%d%%", humValue);
  lv_label_set_text(ui_HumDesired, buffer);
  lv_label_set_text(ui_Label24, buffer);

  snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValueDetected);
  lv_label_set_text(ui_TempAirDetected, buffer);
  lv_label_set_text(ui_TempAirDetectedRight, buffer);
  lv_label_set_text(ui_Label18, buffer);
  snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValueDetected);
  lv_label_set_text(ui_TempSkinDetected, buffer);
  lv_label_set_text(ui_TempSkinDetectedRight, buffer);
  lv_label_set_text(ui_Label14, buffer);
  snprintf(buffer, sizeof(buffer), "%d%%", humValueDetected);
  lv_label_set_text(ui_HumDetected, buffer);
  lv_label_set_text(ui_HumDetectedRight, buffer);
  lv_label_set_text(ui_Label20, buffer);

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
      snprintf(buf, sizeof(buf), "%d:%02d", photoTimerMinutes / 60, photoTimerMinutes % 60);
      lv_label_set_text(ui_PhotoTimeValueLabel, buf);
  }
}

const char* getConnectivityString(int status, ui_lang_t lang) {
    switch (status) {
        case COMM_STATUS_NONE:
            if (lang == LANG_ES) return "DESCONECTADO";
            if (lang == LANG_FR) return "DECONNECTE";
            return "DISCONNECTED";
        case COMM_STATUS_GPRS_ONLY:
            if (lang == LANG_ES) return "2G (SIN SERVER)";
            if (lang == LANG_FR) return "2G (SANS SERVEUR)";
            return "2G (NO SERVER)";
        case COMM_STATUS_GPRS_SERVER:
            if (lang == LANG_ES) return "2G + SERVIDOR";
            if (lang == LANG_FR) return "2G + SERVEUR";
            return "2G + SERVER";
        case COMM_STATUS_WIFI_ONLY:
            if (lang == LANG_ES) return "WIFI (SIN SERVER)";
            if (lang == LANG_FR) return "WIFI (SANS SERVEUR)";
            return "WIFI (NO SERVER)";
        case COMM_STATUS_WIFI_SERVER:
            if (lang == LANG_ES) return "WIFI + SERVIDOR";
            if (lang == LANG_FR) return "WIFI + SERVEUR";
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

  const char *TXT_CONTROLTEMP[] = {"TEMPERATURA", "TEMPERATURE",
                                   "TEMPERATURE"};
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
  const char *TXT_HMI_VERSION[] = {"VERSION PANTALLA:", "DISPLAY VERSION:", "VERSION ECRAN:"};
  const char *TXT_MB_VERSION[] = {"VERSION PLACA:", "MOTHERBOARD VERSION:", "VERSION CARTE:"};
  const char *TXT_SN[] = {"S/N:", "S/N:", "S/N:"};
  const char *TXT_HEATER_ERROR_RESTART[] = {"Error calentador\nToque para mas informacion", "Heater error\nTouch for more information", "Erreur chauffage\nToucher pour plus d'infos"};
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
  const char *TXT_AIRTEMP[] = {"TEMPERATURA AIRE:", "AIR TEMPERATURE:",
                               "AIR TEMPERATURE:"};
  const char *TXT_BABYTEMP[] = {"TEMPERATURA BEBE:", "BABY TEMPERATURE:",
                                "TEMPERATURE BEBE:"};
  const char *TXT_HUM[] = {"HUMEDAD:", "HUMIDITY:", "HUMIDITE:"};
  const char *TXT_TARGETTEMP[] = {"TEMP. OBJETIVO:", "TARGET TEMP:",
                                  "TEMP. OBJECTIF:"};
  const char *TXT_TARGETHUM[] = {"HUMEDAD OBJETIVO:", "TARGET HUMIDITY:",
                                 "HUMIDITE OBJECTIF:"};
  const char *TXT_STATUS[] = {"ESTADO:", "STATUS:", "ETAT:"};
  const char *TXT_UNLOCK[] = {"PRESIONA 2 SEG\nPARA DESBLOQUEAR",
                              "PRESS 2 SEC \nTO UNLOCK",
                              "APPUYEZ 2 SEG\nPOUR DEVERROUILLER"};
  const char *TXT_INCUNEST[] = {"INCUNEST", "INCUNEST", "INCUNEST"};
  const char *TXT_SET[] = {"AJUSTAR", "SET", "REGLER"};
  const char *TXT_WIFISSID[] = {"WIFISSID", "WIFISSID", "WIFISSID"};
  const char *TXT_WIFICONNECTEDTO[] = {"WIFI CONECTADO A", "WIFI CONNECTED TO",
                                       "WIFI CONNECTE A"};
  const char *TXT_ALARMSDESC[] = {"DESCRIPCION DE ALARMAS", "ALARMS DESCRIPTION",
                                  "DESCRIPTION DES ALARMES"};
  const char *TXT_OXICHART[] = {"GRAFICO OXIMETRIA", "OXIMETRY CHART",
                                "GRAPHIQUE OXIMETRIE"};
  const char *TXT_PULSEOXIMETRY[] = {"PULSIOXIMETRIA", "PULSE OXIMETRY",
                                     "PULSIOXYMETRIE"};
  const char *TXT_LANG_OPTIONS[] = {"ESPANOL\nINGLES\nFRANCES",
                                    "SPANISH\nENGLISH\nFRENCH",
                                    "ESPAGNOL\nANGLAIS\nFRANCAIS"};
  const char *TXT_CONNECTIVITY[] = {"CONECTIVIDAD:", "CONNECTIVITY:", "CONNECTIVITE:"};
  const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};

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
  lv_label_set_text(ui_HMIVerTitle, TXT_HMI_VERSION[lang]);
  lv_label_set_text(ui_MBVerTitle, TXT_MB_VERSION[lang]);
  lv_label_set_text(ui_SNTitle, TXT_SN[lang]);
  if (ui_ConnTitle) lv_label_set_text(ui_ConnTitle, TXT_CONNECTIVITY[lang]);
  
  if (ui_HeaterErrorTempLabel) lv_label_set_text(ui_HeaterErrorTempLabel, TXT_HEATER_ERROR_RESTART[lang]);
  if (ui_HeaterErrorHumLabel) lv_label_set_text(ui_HeaterErrorHumLabel, TXT_HEATER_ERROR_RESTART[lang]);

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
      const char *TXT_PHOTO_LOCK[] = {"FOTOTERAPIA:", "PHOTOTHERAPY:", "PHOTOTHERAPIE:"};
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

lv_chart_series_t *configure_temp_chart(lv_obj_t *chart, lv_palette_t pal) {
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
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 20, 45);
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

void set_active_panel(lv_obj_t *active, lv_obj_t *inactive) {
  lv_obj_set_style_bg_color(active, COLOR_PANEL_WHITE, LV_PART_MAIN);
  lv_obj_set_style_opa(active, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(inactive, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(inactive, LV_OPA_COVER, LV_PART_MAIN);
}

void WifiButton_cb(lv_event_t *e) {
  lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  LanguagesVisible = false;
  lv_obj_add_flag(ui_InfoDetailsCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  if (WiFi.status() == WL_CONNECTED) {
    lv_obj_clear_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  }
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
      lv_label_set_text(ui_ConnValue, getConnectivityString(ctrl_state_msg.serverCommStatus, g_lang));
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
  lv_obj_add_flag(ui_ConnectLabel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectButton, LV_OBJ_FLAG_HIDDEN);
}

void Keyboard_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ConnectLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_WifiConnectButton, LV_OBJ_FLAG_HIDDEN);
    const char *txt1 = lv_textarea_get_text(ui_TextArea1);
    const char *txt2 = lv_textarea_get_text(ui_TextArea2);
    strncpy(wifi_ssid, txt1, sizeof(wifi_ssid));
    strncpy(wifi_pass, txt2, sizeof(wifi_pass));
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
  if (ui_AirTempLockCont) lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetAirTempCont) lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_SkinTempLockCont) lv_obj_add_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetSkinTempCont) lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);

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
  if (ui_SkinTempLockCont) lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetSkinTempCont) lv_obj_clear_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_AirTempLockCont) lv_obj_add_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetAirTempCont) lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);

  hmi_msg.controlMode = CONTROL_SKIN;
  hmi_msg.shouldSendData = true;
  temp_chart_show_for_selected_panel();
}

void PhotoTimeMinusBtn_cb(lv_event_t * e) {
    hmi_msg.shouldSendData = true;
    if (photoTimerActive) return; 
    
    if (photoTimerMinutes > 120) {
        photoTimerMinutes -= 20;
    }
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", photoTimerMinutes / 60, photoTimerMinutes % 60);
    lv_label_set_text(ui_PhotoTimeValueLabel, buf);

    // Persist last used timer
    EEPROM.write(EEPROM_PHOTO_TIMER_MINUTES, photoTimerMinutes);
    eepromDirty = true;
    lastVarChangeTime = millis();
}

void PhotoTimePlusBtn_cb(lv_event_t * e) {
    hmi_msg.shouldSendData = true;
    if (photoTimerActive) return;
    
    if (photoTimerMinutes < 600) {
        photoTimerMinutes += 20;
    }
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", photoTimerMinutes / 60, photoTimerMinutes % 60);
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

  // Update visual state via SyncAll (this handles colors, labels and "X" visibility)
  UI_SyncAll();
}

/* Switch callback for temperature and humidity */
void Switch_cb(lv_event_t *e) {
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
      lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_WHITE,
                                LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_WHITE,
                                LV_PART_MAIN);
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
      lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_GRAY,
                                LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);

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
      lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_WHITE,
                                LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_WHITE, LV_PART_MAIN);
    } else {
      lv_obj_add_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN); // hide hum chart
      // Humidity OFF
      lv_obj_clear_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_GRAY,
                                LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_GRAY, LV_PART_MAIN);

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

      lv_obj_set_style_bg_color(ui_PhotoTimerPanel, COLOR_PANEL_WHITE,
                                LV_PART_MAIN);
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
        snprintf(buf, sizeof(buf), "%d:%02d", photoTimerMinutes / 60, photoTimerMinutes % 60);
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

      lv_obj_set_style_bg_color(ui_PhotoTimerPanel, COLOR_PANEL_GRAY,
                                LV_PART_MAIN);
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
        
        if (ui_AirTempLockCont) lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
        if (ui_TargetAirTempCont) lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
        if (ui_SkinTempLockCont) lv_obj_add_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
        if (ui_TargetSkinTempCont) lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);

        // Update control mode if temperature is switched on
        if (tempSwitched) { // only if temp is ON
          hmi_msg.controlMode = CONTROL_AIR;
          temp_chart_show_for_selected_panel();
        }
      }
    }
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
          lv_obj_set_style_transform_zoom(target, 280, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256, LV_PART_MAIN | LV_STATE_DEFAULT);
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
          lv_obj_set_style_transform_zoom(target, 280, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256, LV_PART_MAIN | LV_STATE_DEFAULT);
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
          lv_obj_set_style_transform_zoom(target, 280, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256, LV_PART_MAIN | LV_STATE_DEFAULT);
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
          lv_obj_set_style_transform_zoom(target, 280, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256, LV_PART_MAIN | LV_STATE_DEFAULT);
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

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarmList[i].state) {
      totalActiveAlarms++;
      if (activeCount < MAX_ALARM_DISPLAY) {
        alarmSlotToIndex[activeCount] = i;
        activeCount++;
      }
    }
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
  
  if(!heaterCriticalError) {
      for(int i=0; i<MAX_ALARMS; i++) {
        if(alarmList[i].id == HEATER_ISSUE_ALARM && alarmList[i].state) {
            heaterCriticalError = true;
            break;
        }
      }
  }

  if(heaterCriticalError) {
    // Show Warning UI
    if(ui_HeaterErrorTempCont) lv_obj_clear_flag(ui_HeaterErrorTempCont, LV_OBJ_FLAG_HIDDEN);
    if(ui_HeaterErrorHumCont) lv_obj_clear_flag(ui_HeaterErrorHumCont, LV_OBJ_FLAG_HIDDEN);
    
    // Disable Switches
    if(ui_Switch1) {
        lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
        lv_obj_add_state(ui_Switch1, LV_STATE_DISABLED);
    }
    if(ui_Switch2) {
        lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
        lv_obj_add_state(ui_Switch2, LV_STATE_DISABLED);
    }

    // Blink - Blink the CONTAINER for visibility
    if(ui_HeaterErrorTempCont) start_alarm_blink(ui_HeaterErrorTempCont);
    if(ui_HeaterErrorHumCont) start_alarm_blink(ui_HeaterErrorHumCont);

  } else {
    // Hide Warning UI
    if(ui_HeaterErrorTempCont) lv_obj_add_flag(ui_HeaterErrorTempCont, LV_OBJ_FLAG_HIDDEN);
    if(ui_HeaterErrorHumCont) lv_obj_add_flag(ui_HeaterErrorHumCont, LV_OBJ_FLAG_HIDDEN);

    // Enable Switches
    if(ui_Switch1) lv_obj_clear_state(ui_Switch1, LV_STATE_DISABLED);
    if(ui_Switch2) lv_obj_clear_state(ui_Switch2, LV_STATE_DISABLED);
  }
}

// Event handler for Heater Error Click
static void HeaterError_event_handler(lv_event_t * e) {
    // Go to Alarms Screen
    _ui_screen_change(&ui_ScreenAlarms, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenAlarms_screen_init);
    lv_tabview_set_act(ui_AlarmsTabview, 1, LV_ANIM_OFF); // Go to Description Tab (Page 1)
    
    // Set specific description
    const char *TXT_HEATER_ERROR_DESC[] = {"Hay un problema con el ventilador / calefactor: \n\n 1. Pruebe a desconectar y conectar el calefactor y posteriormente reinicie incubadora\n\n 2. Si no funciona el paso 1, arregle el calefactor.", "There is a problem with the fan / heater:\n\n 1. Try unplugging and plugging in the heater and then restart the incubator\n\n 2. If step 1 doesn't work, fix the heater.", "Il y a un probleme avec le ventilateur / chauffage :\n\n 1. Essayez de debrancher et de brancher le chauffage et redemarrez l'incubateur\n\n 2. Si le pas 1 ne fonctionne pas, reparez le chauffage."};
    if(ui_AlarmDetailLabel) {
        lv_label_set_text(ui_AlarmDetailLabel, TXT_HEATER_ERROR_DESC[g_lang]);
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
}

void Alarm1Cont_cb(lv_event_t *e) { show_alarm_detail_from_slot(0); hmi_msg.shouldSendData = true;
}
void Alarm2Cont_cb(lv_event_t *e) { show_alarm_detail_from_slot(1); hmi_msg.shouldSendData = true;
}
void Alarm3Cont_cb(lv_event_t *e) { show_alarm_detail_from_slot(2); hmi_msg.shouldSendData = true;
}
void Alarm4Cont_cb(lv_event_t *e) { show_alarm_detail_from_slot(3); hmi_msg.shouldSendData = true;
}

void AlarmButton_cb(lv_event_t *e) {
  lv_tabview_set_act(ui_AlarmsTabview, 0, LV_ANIM_ON);
  hmi_msg.shouldSendData = true;

}

void AlarmsTabview_cb(lv_event_t *e) {
  lv_obj_t *tv = lv_event_get_target(e);
  uint16_t act = lv_tabview_get_tab_act(tv);
  if (act == 0) {
    lv_label_set_text(ui_AlarmDetailLabel, "");
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

void AlarmSound_Update() {
  //if (alarmActive && !alarmsMuted)
  //  buzzerOn();
  //else
  // El display ya no emite sonido por alarmas, solo la motherboard.
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
  if (ui_AirTempLockCont) lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_SkinTempLockCont) lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_HumLockCont) lv_obj_clear_flag(ui_HumLockCont, LV_OBJ_FLAG_HIDDEN);
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
    unlockTimeoutTimer = lv_timer_create(unlock_timeout_cb, UNLOCK_TIMEOUT_MS, NULL);
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
  if (alarmActive) {
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
  _ui_screen_change(&ui_ScreenSettings, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenSettings_screen_init);
}

void SPO2Button_cb(lv_event_t *e) {
  _ui_screen_change(&ui_ScreenPulseOxi, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenPulseOxi_screen_init);
}

void ChartButton_cb(lv_event_t *e) {
  _ui_screen_change(&ui_ScreenCharts, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenCharts_screen_init);
}


void AlarmLockImg_cb(lv_event_t *e) {
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
    if(ui_AudioPlayBtn) {
        lv_obj_add_state(ui_AudioPlayBtn, LV_STATE_DISABLED);
    }
}

void AudioStopBtn_cb(lv_event_t *e) {
    AudioManager::getInstance().stop();
    if(ui_AudioPlayBtn) {
        lv_obj_clear_state(ui_AudioPlayBtn, LV_STATE_DISABLED);
    }
}

// ==========================================
// Main Task
// ==========================================
void UI_Task(void *pvParameters) {
  ESP_LOGI(TAG, "UI Task Started");

  // CrowPanel Backlight Control v1.3 (I2C 0x30)
  {
      vTaskDelay(pdMS_TO_TICKS(100));
      
      // Según doc v1.3: 0 es Brillo Máximo, 245 es Apagado.
      Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
      Wire.write(10); // Valor bajo para brillo alto en v1.3
      Wire.endTransmission();
      
      vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Display initialization
  lcd.sc7277_init();
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
  
  /* Comentado para v1.3 (Control vía I2C)
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(TFT_BL_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, BRIGHTNESS_MAX);
  */

  UI_ApplyLanguage(g_lang);

  // Botón de PLAY de Audio
  ui_AudioPlayBtn = lv_btn_create(ui_ScreenSettings);
  lv_obj_set_size(ui_AudioPlayBtn, 120, 50);
  lv_obj_align(ui_AudioPlayBtn, LV_ALIGN_BOTTOM_RIGHT, -160, -20); // Más a la izquierda
  ui_AudioPlayLabel = lv_label_create(ui_AudioPlayBtn);
  lv_label_set_text(ui_AudioPlayLabel, "Play Audio");
  lv_obj_center(ui_AudioPlayLabel);
  lv_obj_add_event_cb(ui_AudioPlayBtn, AudioTestBtn_cb, LV_EVENT_CLICKED, NULL);

  // Botón de STOP de Audio
  ui_AudioStopBtn = lv_btn_create(ui_ScreenSettings);
  lv_obj_set_size(ui_AudioStopBtn, 120, 50);
  lv_obj_align(ui_AudioStopBtn, LV_ALIGN_BOTTOM_RIGHT, -20, -20); // A la derecha
  lv_obj_set_style_bg_color(ui_AudioStopBtn, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT); // Rojo
  lv_obj_t *audioStopLabel = lv_label_create(ui_AudioStopBtn);
  lv_label_set_text(audioStopLabel, "Stop");
  lv_obj_center(audioStopLabel);
  lv_obj_add_event_cb(ui_AudioStopBtn, AudioStopBtn_cb, LV_EVENT_CLICKED, NULL);

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

  lv_textarea_set_text(ui_TextArea1, wifi_ssid);
  lv_textarea_set_text(ui_TextArea2, wifi_pass);

  lv_obj_set_style_bg_color(ui_Panel2, COLOR_PANEL_WHITE, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel2, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel5, COLOR_PANEL_WHITE, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel5, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel6, COLOR_PANEL_WHITE, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel6, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel4, COLOR_PANEL_WHITE, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel4, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_Panel1, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel3, COLOR_PANEL_GRAY, LV_PART_MAIN);
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

  lv_obj_add_flag(ui_ChartButton, LV_OBJ_FLAG_HIDDEN);
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

  airTempSeries = configure_temp_chart(ui_AirTempChart, LV_PALETTE_BLUE);
  skinTempSeries = configure_temp_chart(ui_SkinTempChart, LV_PALETTE_BLUE);

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
  lv_chart_set_range(ui_HumChart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
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
  if(ui_HeaterErrorTempCont) lv_obj_add_event_cb(ui_HeaterErrorTempCont, HeaterError_event_handler, LV_EVENT_CLICKED, NULL);
  if(ui_HeaterErrorHumCont) lv_obj_add_event_cb(ui_HeaterErrorHumCont, HeaterError_event_handler, LV_EVENT_CLICKED, NULL);
  if(ui_HeaterErrorTempLabel) lv_obj_add_event_cb(ui_HeaterErrorTempLabel, HeaterError_event_handler, LV_EVENT_CLICKED, NULL);
  if(ui_HeaterErrorHumLabel) lv_obj_add_event_cb(ui_HeaterErrorHumLabel, HeaterError_event_handler, LV_EVENT_CLICKED, NULL);

  lv_timer_create(inactivity_timer_cb, 1000, NULL);

  for (;;) {
    lv_timer_handler();
    
    // Gestión de estado de botones de Audio
    if(ui_AudioPlayBtn) {
        if(AudioManager::getInstance().isPlaying()) {
            if(!lv_obj_has_state(ui_AudioPlayBtn, LV_STATE_DISABLED)) {
                lv_obj_add_state(ui_AudioPlayBtn, LV_STATE_DISABLED);
            }
        } else {
            if(lv_obj_has_state(ui_AudioPlayBtn, LV_STATE_DISABLED)) {
                lv_obj_clear_state(ui_AudioPlayBtn, LV_STATE_DISABLED);
            }
        }
    }

    // AudioManager::getInstance().loop(); // Ahora corre en su propia tarea (Core 0)

    // Debug Pulse
    static uint32_t lastLoopMs = 0;
    if (millis() - lastLoopMs > 5000) {
        lastLoopMs = millis();
        ESP_LOGD(TAG, "UI and Audio Loop active");
    }
    vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));

    if (wifiVisible) {
      lv_obj_clear_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
      if (WiFi.status() == WL_CONNECTED) {
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
            int totalMins = (remaining + 59) / 60; // Round up to show minutes correctly
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
    } else {
        lastPhotoMinutesSent = -1;
        // Timer not active, hide lock screen container
        if (ui_PhotoLockCont) {
            lv_obj_add_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
        }
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
  // 1. Update internal state flags from current switch visual state
  switchTemp = lv_obj_has_state(ui_Switch1, LV_STATE_CHECKED);
  tempSwitched = switchTemp;
  switchHum = lv_obj_has_state(ui_Switch2, LV_STATE_CHECKED);
  humSwitched = switchHum;
  skinPanelEnabled = lv_obj_has_state(ui_Switch4, LV_STATE_CHECKED);

  // 2. Temperature Logic
  if (tempSwitched) {
    if (selectedPanel == NO_PANEL_SELECTED) {
      selectedPanel = (lastSelectedPanel != NO_PANEL_SELECTED) ? lastSelectedPanel : AIR_PANEL_SELECTED;
    }
    
    if (selectedPanel == AIR_PANEL_SELECTED) {
      set_active_panel(ui_AirPanel, ui_SkinPanel);
      lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
      
      // Target Screen Visibility
      if (ui_TargetAirTempCont) lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowAirLock) lv_obj_clear_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
      if (ui_TargetSkinTempCont) lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowSkinLock) lv_obj_add_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);

      hmi_msg.controlMode = CONTROL_AIR;
    } else {
      set_active_panel(ui_SkinPanel, ui_AirPanel);
      lv_obj_clear_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
      
      // Target Screen Visibility
      if (ui_TargetSkinTempCont) lv_obj_clear_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowSkinLock) lv_obj_clear_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);
      if (ui_TargetAirTempCont) lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowAirLock) lv_obj_add_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);

      hmi_msg.controlMode = CONTROL_SKIN;
    }

    // First Column (Detected Values) ALWAYS VISIBLE
    if (ui_AirTempLockCont) lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_SkinTempLockCont) lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_HumLockCont) lv_obj_clear_flag(ui_HumLockCont, LV_OBJ_FLAG_HIDDEN);

    // Enable arrows
    arrowsActive = true;
    lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_WHITE, LV_PART_MAIN);
    
    // Temperature Panel background
    lv_obj_set_style_bg_color(ui_Panel1, COLOR_PANEL_WHITE, LV_PART_MAIN);
    
    temp_chart_show_for_selected_panel();
  } else {
    selectedPanel = NO_PANEL_SELECTED;
    arrowsActive = false;
    lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Panel1, COLOR_PANEL_GRAY, LV_PART_MAIN);

    // Visually show labels and thermometer of the last selected panel even if OFF
    if (lastSelectedPanel == SKIN_PANEL_SELECTED) {
      lv_obj_clear_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
    }

    // Lock Screen hide targets but keep detected values visible
    if (ui_TargetAirTempCont) lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_TargetSkinTempCont) lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_ArrowAirLock) lv_obj_add_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
    if (ui_ArrowSkinLock) lv_obj_add_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);

    if (ui_AirTempLockCont) lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_SkinTempLockCont) lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_HumLockCont) lv_obj_clear_flag(ui_HumLockCont, LV_OBJ_FLAG_HIDDEN);
  }

  // 3. Humidity Logic
  if (switchHum) {
    lv_obj_clear_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Panel3, COLOR_PANEL_WHITE, LV_PART_MAIN);
  } else {
    lv_obj_add_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Panel3, COLOR_PANEL_GRAY, LV_PART_MAIN);
  }

  // 4. Phototherapy Logic (Switch 3)
  bool photoOn = lv_obj_has_state(ui_Switch3, LV_STATE_CHECKED);
  
  if (photoOn) {
      // Normal state
      lv_obj_set_style_bg_color(ui_PhotoTimerPanel, COLOR_PANEL_WHITE, LV_PART_MAIN);
      lv_obj_add_flag(ui_PhotoTimeMinusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(ui_PhotoTimePlusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(ui_PhotoStartBtn, LV_OBJ_FLAG_CLICKABLE);
      
      // Restore button colors
      lv_obj_set_style_bg_color(ui_PhotoTimeMinusBtn, lv_color_hex(0x0075EE), LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_PhotoTimePlusBtn, lv_color_hex(0x0075EE), LV_PART_MAIN);

      if (photoTimerActive) {
          // Update visual running state
          const char *TXT_RUNNING[] = {"EJECUTANDO", "RUNNING", "EN COURS"};
          if (ui_PhotoStartLabel) lv_label_set_text(ui_PhotoStartLabel, TXT_RUNNING[g_lang]);
          if (ui_PhotoStartBtn) lv_obj_set_style_bg_color(ui_PhotoStartBtn, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT);
          
          // Ensure cancel button is visible
          if (ui_PhotoCancelBtn) lv_obj_clear_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);

          // Ensure lock screen is visible if we are in lock screen
          if (ui_PhotoLockCont) lv_obj_clear_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
      } else {
          // Setup state
          const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
          if (ui_PhotoStartLabel) lv_label_set_text(ui_PhotoStartLabel, TXT_PHOTOSTART[g_lang]);
          if (ui_PhotoStartBtn) lv_obj_set_style_bg_color(ui_PhotoStartBtn, lv_color_hex(0x00AA00), LV_PART_MAIN | LV_STATE_DEFAULT);
          
          // Ensure cancel button is hidden
          if (ui_PhotoCancelBtn) lv_obj_add_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);

          if (ui_PhotoLockCont) lv_obj_add_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);

          // Restore normal minutes display if not active
          char buf[16];
          snprintf(buf, sizeof(buf), "%d min", photoTimerMinutes);
          if (ui_PhotoTimeValueLabel) lv_label_set_text(ui_PhotoTimeValueLabel, buf);
      }
  } else {
      // Grayed out state
      lv_obj_set_style_bg_color(ui_PhotoTimerPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
      lv_obj_clear_flag(ui_PhotoTimeMinusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_PhotoTimePlusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_PhotoStartBtn, LV_OBJ_FLAG_CLICKABLE);
      
      // Buttons to light gray as requested
      lv_obj_set_style_bg_color(ui_PhotoTimeMinusBtn, COLOR_PANEL_LIGHT_GRAY, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_PhotoTimePlusBtn, COLOR_PANEL_LIGHT_GRAY, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_PhotoStartBtn, COLOR_PANEL_LIGHT_GRAY, LV_PART_MAIN);

      // Reset visual values
      if (!photoTimerActive) {
          char buf[16];
          snprintf(buf, sizeof(buf), "%d min", photoTimerMinutes);
          if (ui_PhotoTimeValueLabel) lv_label_set_text(ui_PhotoTimeValueLabel, buf);

          const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
          if (ui_PhotoStartLabel) lv_label_set_text(ui_PhotoStartLabel, TXT_PHOTOSTART[g_lang]);
      }

      if (ui_PhotoCancelBtn) lv_obj_add_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);
      if (ui_PhotoLockCont) lv_obj_add_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
      photoTimerActive = false; // Safety
  }
  
  // Panel is ALWAYS white as requested
  lv_obj_set_style_bg_color(ui_Panel2, COLOR_PANEL_WHITE, LV_PART_MAIN);
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
      if (switchTemp) lv_obj_clear_flag(temp_tab_btn, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(temp_tab_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (hum_tab_btn) {
      if (switchHum) lv_obj_clear_flag(hum_tab_btn, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(hum_tab_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if ((switchTemp && !switchHum) || (!switchTemp && switchHum)) {
      if (tab_btns_cont) lv_obj_add_flag(tab_btns_cont, LV_OBJ_FLAG_HIDDEN);
    } else {
      if (tab_btns_cont) lv_obj_clear_flag(tab_btns_cont, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (switchTemp && !switchHum) lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_OFF);
    else if (!switchTemp && switchHum) lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_OFF);
  }

  update_labels();
}


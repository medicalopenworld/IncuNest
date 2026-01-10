#include "main.h"

#include "communication.h"
#include "esp_log.h"
#include <PCA9557.h>
#include <lvgl.h>
// #include <DHT20.h>

static const char *TAG = "Main";
#include "ui.h"
#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <SPI.h>
#include <TAMC_GT911.h>
#include <buzzer.h>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

#include <WiFi.h>

// Temperature and humidity variables
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
static bool alarmsMuted = false;

bool prevTempAlarm = false;
bool prevHumAlarm = false;
int alarmSlotToIndex[MAX_ALARM_DISPLAY] = {-1, -1, -1, -1};

// Chart last pressed: 0=temp, 1=hum, -1=none
int chartLastPressed = -1;

bool wifiVisible = false;
char wifi_ssid[64] = "";
char wifi_pass[64] = "";
bool LanguagesVisible = false;
bool locked = true;

lv_chart_series_t *airTempSeries = NULL;
lv_chart_series_t *skinTempSeries = NULL;
lv_chart_series_t *humSeries = NULL;

static bool g_stateSynced = false;
static uint32_t g_lastStateReqMs = 0;

static ui_lang_t g_lang = LANG_ES;

struct Alarm {
  int id;
  char type[ALARM_TYPE_LEN];
  char description[ALARM_DESC_LEN];
  bool state;
};
Alarm alarmList[MAX_ALARMS]; // Array to store up to MAX_ALARMS alarms

class LGFX : public lgfx::LGFX_Device {
public:
  lgfx::Bus_RGB _bus_instance;
  lgfx::Panel_RGB _panel_instance;

  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;

      cfg.pin_d0 = GPIO_NUM_15; // B0
      cfg.pin_d1 = GPIO_NUM_7;  // B1
      cfg.pin_d2 = GPIO_NUM_6;  // B2
      cfg.pin_d3 = GPIO_NUM_5;  // B3
      cfg.pin_d4 = GPIO_NUM_4;  // B4

      cfg.pin_d5 = GPIO_NUM_9;  // G0
      cfg.pin_d6 = GPIO_NUM_46; // G1
      cfg.pin_d7 = GPIO_NUM_3;  // G2
      cfg.pin_d8 = GPIO_NUM_8;  // G3
      cfg.pin_d9 = GPIO_NUM_16; // G4
      cfg.pin_d10 = GPIO_NUM_1; // G5

      cfg.pin_d11 = GPIO_NUM_14; // R0
      cfg.pin_d12 = GPIO_NUM_21; // R1
      cfg.pin_d13 = GPIO_NUM_47; // R2
      cfg.pin_d14 = GPIO_NUM_48; // R3
      cfg.pin_d15 = GPIO_NUM_45; // R4

      cfg.pin_henable = GPIO_NUM_41;
      cfg.pin_vsync = GPIO_NUM_40;
      cfg.pin_hsync = GPIO_NUM_39;
      cfg.pin_pclk = GPIO_NUM_0;
      cfg.freq_write = CFG_FREQ_WRITE;

      cfg.hsync_polarity = CFG_HSYNC_POLARITY;
      cfg.hsync_front_porch = CFG_HSYNC_FRONT_PORCH;
      cfg.hsync_pulse_width = CFG_HSYNC_PULSE_WIDTH;
      cfg.hsync_back_porch = CFG_HSYNC_BACK_PORCH;

      cfg.vsync_polarity = CFG_VSYNC_POLARITY;
      cfg.vsync_front_porch = CFG_VSYNC_FRONT_PORCH;
      cfg.vsync_pulse_width = CFG_VSYNC_PULSE_WIDTH;
      cfg.vsync_back_porch = CFG_VSYNC_BACK_PORCH;

      cfg.pclk_active_neg = CFG_PCLK_ACTIVE_NEG;
      cfg.de_idle_high = CFG_DE_IDLE_HIGH;
      cfg.pclk_idle_high = CFG_PCLK_IDLE_HIGH;

      _bus_instance.config(cfg);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width = CFG_MEMORY_WIDTH;
      cfg.memory_height = CFG_MEMORY_HEIGHT;
      cfg.panel_width = CFG_PANEL_WIDTH;
      cfg.panel_height = CFG_PANEL_HEIGHT;
      cfg.offset_x = CFG_OFFSET_X;
      cfg.offset_y = CFG_OFFSET_Y;
      _panel_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);
  }
};
LGFX lcd;

// Touchscreen
#define TOUCH_SDA TOUCH_SDA_PIN
#define TOUCH_SCL TOUCH_SCL_PIN
#define TOUCH_INT TOUCH_INT_PIN
#define TOUCH_RST TOUCH_RST_PIN // use -1 if you don't have reset pin

#define TOUCH_WIDTH DISPLAY_WIDTH
#define TOUCH_HEIGHT DISPLAY_HEIGHT

TAMC_GT911 ts = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST,
                           TOUCH_WIDTH, TOUCH_HEIGHT);

// UI
#define TFT_BL TFT_BL_PIN
int led;
// DHT20 dht20;
SPIClass &spi = SPI;

/* Screen resolution */
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf[DISPLAY_WIDTH * DISPLAY_HEIGHT / COLOR_DIVISOR];
static lv_disp_drv_t disp_drv;
static lv_timer_t *intro_timer = NULL;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                   lv_color_t *color_p) {

  uint32_t w = (area->x2 - area->x1 + AREA_PIXEL_OFFSET);
  uint32_t h = (area->y2 - area->y1 + AREA_PIXEL_OFFSET);

// lcd.fillScreen(TFT_WHITE);
#if (LV_COLOR_16_SWAP != 0)
  lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t *)&color_p->full);
#else
  lcd.pushImageDMA(area->x1, area->y1, w, h,
                   (lgfx::rgb565_t *)&color_p->full); //
#endif

  lv_disp_flush_ready(disp);
}

static void intro_timer_cb(lv_timer_t *t) {
  (void)t;
  // Changes to main screen and deletes the timer
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

/* Update labels for temperature and humidity */
void update_labels() {
  char buffer[BUFFER_SIZE];

  snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValue);
  lv_label_set_text(ui_TempAirDesired, buffer);
  lv_label_set_text(ui_TargetAirTempNumLabel,
                    buffer); // TargetAir numeric label

  snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValue);
  lv_label_set_text(ui_TempSkinDesired, buffer);
  lv_label_set_text(ui_TargetSkinTempNumLabel,
                    buffer); // TargetSkin numeric label

  snprintf(buffer, sizeof(buffer), "%d%%", humValue);
  lv_label_set_text(ui_HumDesired, buffer);

  lv_label_set_text(ui_Label24, buffer); // Label24 shows desired humidity too

  if (airTempValueDetected != 0 || skinTempValueDetected != 0 ||
      humValueDetected != 0) {
    snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValueDetected);
    lv_label_set_text(ui_TempAirDetected, buffer);
    lv_label_set_text(ui_TempAirDetectedRight, buffer);
    lv_label_set_text(ui_Label18, buffer); // Label24 = TempAirDetected

    snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValueDetected);
    lv_label_set_text(ui_TempSkinDetected, buffer);
    lv_label_set_text(ui_TempSkinDetectedRight, buffer);
    lv_label_set_text(ui_Label14, buffer); // Label14 = TempSkinDetected

    snprintf(buffer, sizeof(buffer), "%d%%", humValueDetected);
    lv_label_set_text(ui_HumDetected, buffer);
    lv_label_set_text(ui_HumDetectedRight, buffer);
    lv_label_set_text(ui_Label20, buffer); // Label20 = HumDetected

    double tAir = airTempValueDetected;
    double tSkin = skinTempValueDetected;

    // Air
    int airBar;
    if (tAir <= 20.0)
      airBar = 0;
    else if (tAir >= 40.0)
      airBar = 20;
    else
      airBar = (int)round(tAir - 20.0);

    // Skin
    int skinBar;
    if (tSkin <= 20.0)
      skinBar = 0;
    else if (tSkin >= 40.0)
      skinBar = 20;
    else
      skinBar = (int)round(tSkin - 20.0);

    // Humidity
    int humBar = constrain(humValueDetected, 0, 100);

    lv_bar_set_value(ui_AirTempBar, airBar, LV_ANIM_OFF);
    lv_bar_set_value(ui_SkinTempBar, skinBar, LV_ANIM_OFF);
    lv_bar_set_value(ui_HumBar, humBar, LV_ANIM_OFF);
  }
}

// Configure LANGUAGES

static void UI_ApplyLanguage(ui_lang_t lang) {
  g_lang = lang;

  // MAIN SCREEN
  const char *TXT_CONTROLTEMP[] = {"Control Temperatura", "Temperature Control",
                                   "Controle Temperature"};
  const char *TXT_CONTROLHUM[] = {"Control Humedad", "Humidity Control",
                                  "Controle Humidite"};
  const char *TXT_PHOTO[] = {"Fototerapia", "Phototherapy", "Phototherapie"};
  const char *TXT_AIR[] = {"Aire", "Air", "Air"};
  const char *TXT_SKIN[] = {"Piel", "Skin", "Peau"};
  const char *TXT_ON[] = {"ON", "ON", "ON"};
  const char *TXT_OFF[] = {"OFF", "OFF", "OFF"};

  // SETTINGS SCREEN
  const char *TXT_SETTINGS[] = {"Ajustes", "Settings", "Parametres"};
  const char *TXT_LANG[] = {"Idioma", "Language", "Langue"};
  const char *TXT_WIFI[] = {"WiFi", "WiFi", "WiFi"};
  const char *TXT_CONNECT[] = {"Conectar", "Connect", "Connexion"};
  const char *TXT_SSID[] = {"SSID", "SSID", "SSID"};
  const char *TXT_PASSWORD[] = {"Contraseña", "Password", "Mot de passe"};
  const char *TXT_SKINMODE[] = {"Modo Piel", "Skin Mode", "Mode Peau"};

  // ALARMS SCREEN

  const char *TXT_ALARMS[] = {"Alarmas", "Alarms", "Alarmes"};
  const char *TXT_VIEWDETAIL[] = {"Ver detalles", "View details",
                                  "Voir details"};
  const char *TXT_MUTE[] = {"Silenciar", "Mute", "Muet"};

  // CHART SCREEN
  const char *TXT_HUMCHART[] = {"Grafico Humedad", "Humidity Chart",
                                "Graphique Humidite"};
  const char *TXT_AIRTEMPCHART[] = {"Grafico Temperatura Aire",
                                    "Air Temperature Chart",
                                    "Graphique Temperature Air"};
  const char *TXT_SKINTEMPCHART[] = {"Grafico Temperatura Piel",
                                     "Skin Temperature Chart",
                                     "Graphique Temperature Peau"};
  const char *TXT_TABTEMP[] = {"Temperatura", "Temperature", "Temperature"};
  const char *TXT_TABHUM[] = {"Humedad", "Humidity", "Humidite"};

  // LOCK SCREEN
  const char *TXT_AIRTEMP[] = {"Temperatura Aire", "Air Temperature",
                               "Air Temperature"};
  const char *TXT_BABYTEMP[] = {"Temperatura Bebe", "Baby Temperature",
                                "Temperature Bebe"};
  const char *TXT_HUM[] = {"Humedad", "Humidity", "Humidite"};

  const char *TXT_TARGETTEMP[] = {"Temperatura Objetivo", "Target Temperature",
                                  "Temperature objectif"};
  const char *TXT_TARGETHUM[] = {"Humedad Objetivo", "Target Humidity",
                                 "Humidite objectif"};
  const char *TXT_STATUS[] = {"Estado", "Status", "Etat"};

  const char *TXT_UNLOCK[] = {"PRESIONA 2 SEG\nPARA DESBLOQUEAR",
                              "PRESS 2 SEC TO UNLOCK",
                              "APPUYEZ 2 S\nPOUR DEVERROUILLER"};

  // --- Aplica a los labels estáticos ---
  // (Pon aquí SOLO los que son texto fijo)
  // Ejemplo:
  // lv_label_set_text(ui_SettingsTitleLabel, TXT_SETTINGS[lang]);
  // lv_label_set_text(ui_LangTitleLabel,     TXT_LANG[lang]);
  // lv_label_set_text(ui_WifiTitleLabel,     TXT_WIFI[lang]);
  // lv_label_set_text(ui_ConnectLabel,       TXT_CONNECT[lang]);
  // lv_label_set_text(ui_SSIDLabel,          TXT_SSID[lang]);
  // lv_label_set_text(ui_PasswordLabel,      TXT_PASSWORD[lang]);

  // MAIN SCREEN
  lv_label_set_text(ui_Label2, TXT_CONTROLTEMP[lang]);
  lv_label_set_text(ui_HumidityLabel, TXT_CONTROLHUM[lang]);
  lv_label_set_text(ui_PhototherapyLabel, TXT_PHOTO[lang]);
  lv_label_set_text(ui_Label30, TXT_AIR[lang]);
  lv_label_set_text(ui_Label31, TXT_SKIN[lang]);

  // ON/OFF SWITCHES LABELS
  lv_label_set_text(ui_Label9, TXT_ON[lang]);
  lv_label_set_text(ui_Label15, TXT_OFF[lang]);
  lv_label_set_text(ui_Label13, TXT_ON[lang]);
  lv_label_set_text(ui_Label16, TXT_OFF[lang]);
  lv_label_set_text(ui_Label10, TXT_ON[lang]);
  lv_label_set_text(ui_Label17, TXT_OFF[lang]);

  // SETTINGS SCREEN
  lv_label_set_text(ui_Label8, TXT_SETTINGS[lang]);
  lv_label_set_text(ui_LanguagesLabel, TXT_LANG[lang]);
  lv_label_set_text(ui_WifiLabel, TXT_WIFI[lang]);
  lv_label_set_text(ui_ConnectLabel, TXT_CONNECT[lang]);
  lv_label_set_text(ui_SSIDLabel, TXT_SSID[lang]);
  lv_label_set_text(ui_PassLabel, TXT_PASSWORD[lang]);
  lv_label_set_text(ui_SkinOptionLabel, TXT_SKINMODE[lang]);

  // ALARMS SCREEN
  {
    lv_obj_t *btnm = lv_tabview_get_tab_btns(ui_AlarmsTabview);
    if (btnm && lv_obj_check_type(btnm, &lv_btnmatrix_class)) {
      static const char *map_alarm[3]; // 2 tabs + terminador ""
      map_alarm[0] = TXT_ALARMS[lang];
      map_alarm[1] = TXT_VIEWDETAIL[lang];
      map_alarm[2] = "";
      lv_btnmatrix_set_map(btnm, map_alarm);
    }
  }

  // lv_label_set_text(ui_MuteLabel, TXT_MUTE[lang]);

  // CHART SCREEN
  lv_label_set_text(ui_Label36, TXT_HUMCHART[lang]);
  lv_label_set_text(ui_Label37, TXT_AIRTEMPCHART[lang]);
  lv_label_set_text(ui_Label38, TXT_SKINTEMPCHART[lang]);
  {
    lv_obj_t *btnm = lv_tabview_get_tab_btns(ui_TabView1);
    if (btnm && lv_obj_check_type(btnm, &lv_btnmatrix_class)) {
      static const char *map_chart[3]; // 2 tabs + terminador ""
      map_chart[0] = TXT_TABTEMP[lang];
      map_chart[1] = TXT_TABHUM[lang];
      map_chart[2] = "";
      lv_btnmatrix_set_map(btnm, map_chart);
    }
  }

  // LOCK SCREEN
  lv_label_set_text(ui_Label11, TXT_AIRTEMP[lang]);
  lv_label_set_text(ui_Label12, TXT_BABYTEMP[lang]);
  lv_label_set_text(ui_Label19, TXT_HUM[lang]);
  lv_label_set_text(ui_TargetAirTempLabel, TXT_TARGETTEMP[lang]);
  lv_label_set_text(ui_TargetSkinTempLabel, TXT_TARGETTEMP[lang]);
  lv_label_set_text(ui_Label23, TXT_TARGETHUM[lang]);
  lv_label_set_text(ui_StatusLabel, TXT_STATUS[lang]);
  lv_label_set_text(ui_Label4, TXT_UNLOCK[lang]);

  update_labels();
}

static void LanguagesDropDown_cb(lv_event_t *e) {
  lv_obj_t *dd = lv_event_get_target(e);
  uint16_t sel = lv_dropdown_get_selected(dd); // 0=Spanish, 1=English, 2=French

  if (sel == 0)
    UI_ApplyLanguage(LANG_ES);
  else if (sel == 1)
    UI_ApplyLanguage(LANG_EN);
  else if (sel == 2)
    UI_ApplyLanguage(LANG_FR);
}

static lv_chart_series_t *configure_temp_chart(lv_obj_t *chart,
                                               lv_palette_t pal) {
  // Remove series created by SquareLine
  lv_chart_series_t *s = lv_chart_get_series_next(chart, NULL);
  while (s != NULL) {
    lv_chart_series_t *next = lv_chart_get_series_next(chart, s);
    // Workaround: SquareLine/LVGL creates series with uninitialized x_points
    // for line charts but lv_chart_remove_series tries to free it.
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

static void temp_chart_show_for_selected_panel(void) {
  // Hides both if tempSwitched is false
  if (!tempSwitched) {
    lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  // Tempo ON: ensure selectedPanel is Air as default
  if (selectedPanel != AIR_PANEL_SELECTED &&
      selectedPanel != SKIN_PANEL_SELECTED) {
    selectedPanel = AIR_PANEL_SELECTED;
    lastSelectedPanel = selectedPanel;
    hmi_msg.controlMode = CONTROL_AIR;
  }

  // Hide both charts first
  lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);

  // Show the selected one
  if (selectedPanel == AIR_PANEL_SELECTED) {
    lv_obj_clear_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
  }
}

static void chart_add_air_temp(float v) {
  if (!airTempSeries)
    return;
  lv_chart_set_next_value(ui_AirTempChart, airTempSeries, (lv_coord_t)v);
}

static void chart_add_skin_temp(float v) {
  if (!skinTempSeries)
    return;
  lv_chart_set_next_value(ui_SkinTempChart, skinTempSeries, (lv_coord_t)v);
}

/* Temperature and humidity panel styling */
void set_active_panel(lv_obj_t *active, lv_obj_t *inactive) {
  // Active panel → blue with full opacity
  lv_obj_set_style_bg_color(active, COLOR_PANEL_WHITE, LV_PART_MAIN);
  lv_obj_set_style_opa(active, LV_OPA_COVER, LV_PART_MAIN);

  // Inactive panel → dark gray
  lv_obj_set_style_bg_color(inactive, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(inactive, LV_OPA_COVER, LV_PART_MAIN);
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
    hmi_msg.phototherapyMode = checked ? PHOTOTHERAPY_ON : PHOTOTHERAPY_OFF;
    hmi_msg.shouldSendData = true;
  } else if (obj == ui_Switch4) { // SKIN BLOCK SWITCH
    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);
    skinPanelEnabled = checked;

    if (checked) {
      // show container of skin
      lv_obj_clear_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);

      lv_obj_set_style_bg_color(ui_SkinPanelCont, COLOR_PANEL_WHITE,
                                LV_PART_MAIN);
      lv_obj_set_style_opa(ui_SkinPanelCont, LV_OPA_COVER, LV_PART_MAIN);
    } else {
      // Hide container of skin
      lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);

      if (selectedPanel == SKIN_PANEL_SELECTED) {
        selectedPanel = AIR_PANEL_SELECTED;
        lastSelectedPanel = selectedPanel;

        // Switch active panel to Air
        set_active_panel(ui_AirPanel, ui_SkinPanel);

        // Update control mode if temperature is switched on
        if (tempSwitched) { // only if temp is ON
          hmi_msg.controlMode = CONTROL_AIR;
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

/* Callback when Wifi button is clicked */
void WifiButton_cb(lv_event_t *e) {
  // Hides languages dropdown
  lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  LanguagesVisible = false;

  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);

  // Shows wifi containers (depending on connection status)
  if (WiFi.status() == WL_CONNECTED) {
    lv_obj_clear_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  }
  wifiVisible = true;
}

void LanguageButton_cb(lv_event_t *e) {
  // Hides wifi containers
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  wifiVisible = false;

  // Shows languages
  lv_obj_clear_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  LanguagesVisible = true;
}

/* Callback when pressed TextArea (SSID / Password) */
void TextArea_focus_cb(lv_event_t *e) {
  lv_obj_t *ta = lv_event_get_target(e); // TextArea that triggered the event

  // Show keyboard
  lv_obj_clear_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);

  // Hide Wifi connect button while keyboard is visible
  lv_obj_add_flag(ui_WifiConnectButton, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ConnectLabel, LV_OBJ_FLAG_HIDDEN);

  // Associate keyboard with this TextArea
  lv_keyboard_set_textarea(ui_Keyboard1, ta);
}

/* Callback of the keyboard: hide when OK or Cancel is pressed */
void Keyboard_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *kb = lv_event_get_target(e);

  if (code == LV_EVENT_READY) {
    // OK pulsed
    lv_obj_t *ta = lv_keyboard_get_textarea(kb);
    if (ta != NULL) {
      const char *txt = lv_textarea_get_text(ta);

      if (ta == ui_TextArea1) {
        // SSID
        strncpy(wifi_ssid, txt, sizeof(wifi_ssid));
        wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';
        Serial.print("SSID saved: ");
        Serial.println(wifi_ssid);
      } else if (ta == ui_TextArea2) {
        // PASS
        strncpy(wifi_pass, txt, sizeof(wifi_pass));
        wifi_pass[sizeof(wifi_pass) - 1] = '\0';
        Serial.print("PASS saved: ");
        Serial.println(wifi_pass);
      }
    }
  }
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    // Disassociate textarea
    lv_keyboard_set_textarea(ui_Keyboard1, NULL);
    // Hide keyboard
    lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
    // Show Wifi connect button again
    lv_obj_clear_flag(ui_WifiConnectButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ConnectLabel, LV_OBJ_FLAG_HIDDEN);
  }
}

/* Callback when Air panel is clicked */
void AirPanel_cb(lv_event_t *e) {
  if (!tempSwitched)
    return;
  selectedPanel = AIR_PANEL_SELECTED; // Air panel selected
  lastSelectedPanel = selectedPanel;

  arrowsActive = true;
  set_active_panel(ui_AirPanel, ui_SkinPanel);

  // --- Show only the Air container ---
  lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);

  hmi_msg.controlMode = CONTROL_AIR;
  hmi_msg.desiredAirTemperature = airTempValue;
  hmi_msg.desiredSkinTemperature = skinTempValue;
  hmi_msg.desiredHumidity = humValue;
  hmi_msg.shouldSendData = true;

  update_labels();
  temp_chart_show_for_selected_panel();
}

/* Callback when Skin panel is clicked */
void SkinPanel_cb(lv_event_t *e) {
  if (!tempSwitched)
    return;
  selectedPanel = SKIN_PANEL_SELECTED; // Skin panel selected
  lastSelectedPanel = selectedPanel;

  arrowsActive = true;
  set_active_panel(ui_SkinPanel, ui_AirPanel);

  // --- Show only the Skin container ---
  lv_obj_clear_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);

  hmi_msg.controlMode = CONTROL_SKIN;
  hmi_msg.desiredAirTemperature = airTempValue;
  hmi_msg.desiredSkinTemperature = skinTempValue;
  hmi_msg.desiredHumidity = humValue;
  hmi_msg.shouldSendData = true;

  update_labels();
  temp_chart_show_for_selected_panel();
}

/* Setup panel click callbacks */
void setup_panel_callbacks() {
  lv_obj_add_event_cb(ui_AirPanel, AirPanel_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_SkinPanel, SkinPanel_cb, LV_EVENT_CLICKED, NULL);
}

/* Initialize random values for temperature and humidity */
void init_values() {
  // DESIRED VALUES
  airTempValue = (double)random(RAND_AIR_MIN, RAND_AIR_MAX) /
                 (double)TEMP_DIVISOR; // 20.0 to 36.9 equivalent
  skinTempValue = (double)random(RAND_SKIN_MIN, RAND_SKIN_MAX) /
                  (double)TEMP_DIVISOR; // 35.0 to 37.5 equivalent
  humValue =
      random(RAND_HUM_MIN, RAND_HUM_MAX) *
      HUM_STEP; // Generates values from HUM_MIN to HUM_MAX in steps of HUM_STEP

  char buffer[BUFFER_SIZE];
  snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValue);
  lv_label_set_text(ui_TempAirDesired, buffer);

  snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValue);
  lv_label_set_text(ui_TempSkinDesired, buffer);

  snprintf(buffer, sizeof(buffer), "%d%%", humValue);
  lv_label_set_text(ui_HumDesired, buffer);

  // DETECTED VALUES
  airTempValueDetected = (double)random(RAND_AIR_MIN, RAND_AIR_MAX) /
                         (double)TEMP_DIVISOR; // 20.0 to 36.9 equivalent
  skinTempValueDetected = (double)random(RAND_SKIN_MIN, RAND_SKIN_MAX) /
                          (double)TEMP_DIVISOR; // 35.0 to 37.5 equivalent
  humValueDetected =
      random(RAND_HUM_MIN, RAND_HUM_MAX) *
      HUM_STEP; // Generates values from HUM_MIN to HUM_MAX in steps of HUM_STEP

  snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValueDetected);
  lv_label_set_text(ui_TempAirDetected, buffer);
  lv_label_set_text(ui_TempAirDetectedRight, buffer);

  snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValueDetected);
  lv_label_set_text(ui_TempSkinDetected, buffer);
  lv_label_set_text(ui_TempSkinDetectedRight, buffer);

  snprintf(buffer, sizeof(buffer), "%d%%", humValueDetected);
  lv_label_set_text(ui_HumDetected, buffer);
  lv_label_set_text(ui_HumDetectedRight, buffer);

  update_labels();
}

/* Setup arrow button callbacks for temperature */
void setup_arrow_callbacks() {
  // Up arrow (temperature)
  lv_obj_add_event_cb(
      ui_ImgArrowUpTemp,
      [](lv_event_t *e) {
        if (!tempSwitched || !arrowsActive)
          return;
        if (selectedPanel == AIR_PANEL_SELECTED) {
          airTempValue += TEMP_INCREMENT;
        } else if (selectedPanel == SKIN_PANEL_SELECTED) {
          skinTempValue += TEMP_INCREMENT;
        }
        update_labels();

        hmi_msg.desiredAirTemperature = airTempValue;
        hmi_msg.desiredSkinTemperature = skinTempValue;
        hmi_msg.desiredHumidity = humValue;
        hmi_msg.shouldSendData = true;
      },
      LV_EVENT_CLICKED, NULL);

  // Down arrow (temperature)
  lv_obj_add_event_cb(
      ui_ImgArrowDownTemp,
      [](lv_event_t *e) {
        if (!tempSwitched || !arrowsActive)
          return;
        if (selectedPanel == SKIN_PANEL_SELECTED) {
          skinTempValue -= TEMP_INCREMENT;
        } else if (selectedPanel == AIR_PANEL_SELECTED) {
          airTempValue -= TEMP_INCREMENT;
        }
        update_labels();

        hmi_msg.desiredAirTemperature = airTempValue;
        hmi_msg.desiredSkinTemperature = skinTempValue;
        hmi_msg.desiredHumidity = humValue;
        hmi_msg.shouldSendData = true;
      },
      LV_EVENT_CLICKED, NULL);
}

/* Setup arrow button callbacks for humidity */
void setup_arrow_hum_callbacks() {
  lv_obj_add_event_cb(
      ui_ImgArrowUpHum,
      [](lv_event_t *e) {
        if (!humSwitched)
          return;
        humValue = min(HUM_MAX, humValue + HUM_STEP);
        update_labels();

        hmi_msg.desiredAirTemperature = airTempValue;
        hmi_msg.desiredSkinTemperature = skinTempValue;
        hmi_msg.desiredHumidity = humValue;
        hmi_msg.shouldSendData = true;
      },
      LV_EVENT_CLICKED, NULL);

  lv_obj_add_event_cb(
      ui_ImgArrowDownHum,
      [](lv_event_t *e) {
        if (!humSwitched)
          return;
        humValue = max(HUM_MIN, humValue - HUM_STEP);
        update_labels();

        hmi_msg.desiredAirTemperature = airTempValue;
        hmi_msg.desiredSkinTemperature = skinTempValue;
        hmi_msg.desiredHumidity = humValue;
        hmi_msg.shouldSendData = true;
      },
      LV_EVENT_CLICKED, NULL);
}

/* Animation callback for blinking alarms */
static void blink_cb(void *obj, int32_t v) {
  lv_obj_t *target = (lv_obj_t *)obj;
  lv_obj_set_style_opa(target, v, STYLE_SELECTOR_DEFAULT);
}

/* Start blinking animation on alarm panels */
void start_alarm_blink(lv_obj_t *obj) {

  lv_anim_del(obj, blink_cb);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
  lv_anim_set_time(&a, ANIM_TIME_MS);
  lv_anim_set_playback_time(&a, ANIM_PLAYBACK_MS);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, blink_cb);
  lv_anim_start(&a);
}

/* Update alarm panels according to alarmList */
void update_alarm_panels() {
  int pos = NUM_ALARM_0; // Position index for active alarms

  // Reset alarmSlotToIndex mapping
  for (int s = 0; s < MAX_ALARM_DISPLAY; s++) {
    alarmSlotToIndex[s] = -1;
  }

  int activeCount = 0;

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarmList[i].state) {
      activeCount++;
      switch (pos) {
      case NUM_ALARM_0:
        alarmSlotToIndex[0] = i;
        lv_obj_clear_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_Alarm1Label, alarmList[i].type);
        start_alarm_blink(ui_Alarm1Panel);
        break;
      case NUM_ALARM_1:
        alarmSlotToIndex[1] = i;
        lv_obj_clear_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_Alarm2Label, alarmList[i].type);
        start_alarm_blink(ui_Alarm2Panel);
        break;
      case NUM_ALARM_2:
        alarmSlotToIndex[2] = i;
        lv_obj_clear_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_Alarm3Label, alarmList[i].type);
        start_alarm_blink(ui_Alarm3Panel);
        break;
      case NUM_ALARM_3:
        alarmSlotToIndex[3] = i;
        lv_obj_clear_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_Alarm4Label, alarmList[i].type);
        start_alarm_blink(ui_Alarm4Panel);
        break;
      }
      pos++;
      if (pos >= MAX_ALARM_DISPLAY)
        break;
    }
  }

  // ----- Global alarm state -----
  alarmActive = (activeCount > 0);

  if (alarmActive && !alarmsMuted) {
    lv_obj_clear_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN); // show
  } else {
    lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN); // hide
  }

  // ----- Indicator on main screen: Panel10 + NumAlarm -----
  if (alarmActive) {
    // Text with the number of alarms
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", activeCount);
    lv_label_set_text(ui_NumAlarm, buf);
    lv_label_set_text(ui_AlarmLockNumLabel, buf);

    // Show them and start animations
    lv_obj_clear_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(ui_AlarmLockCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_PanelLockAlarm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_AlarmLockNumLabel, LV_OBJ_FLAG_HIDDEN);

    // Make them blink in unison
    start_alarm_blink(ui_Panel10);
    start_alarm_blink(ui_NumAlarm);
    start_alarm_blink(ui_AlarmButton);

    start_alarm_blink(ui_PanelLockAlarm);
    start_alarm_blink(ui_AlarmLockNumLabel);
    start_alarm_blink(ui_AlarmLockImg);

    lv_obj_add_flag(ui_CheckImg,
                    LV_OBJ_FLAG_HIDDEN); // hide check mark when alarm active
  } else {
    // No alarms: hide and remove animations
    lv_obj_add_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);

    lv_anim_del(ui_Panel10, blink_cb);
    lv_anim_del(ui_NumAlarm, blink_cb);
    lv_anim_del(ui_AlarmButton, blink_cb);
    lv_obj_set_style_opa(ui_Panel10, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_NumAlarm, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_AlarmButton, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_add_flag(ui_AlarmLockCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelLockAlarm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_AlarmLockNumLabel, LV_OBJ_FLAG_HIDDEN);
    lv_anim_del(ui_PanelLockAlarm, blink_cb);
    lv_anim_del(ui_AlarmLockNumLabel, blink_cb);
    lv_anim_del(ui_AlarmLockImg, blink_cb);
    lv_obj_set_style_opa(ui_PanelLockAlarm, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_AlarmLockNumLabel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_AlarmLockImg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(ui_CheckImg,
                      LV_OBJ_FLAG_HIDDEN); // show check mark when no alarms
  }

  // Hide remaining panels if fewer than MAX_ALARM_DISPLAY alarms active
  if (pos < MAX_ALARM_DISPLAY) {
    if (pos <= NUM_ALARM_0) {
      lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN);
    }
    if (pos <= NUM_ALARM_1) {
      lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN);
    }
    if (pos <= NUM_ALARM_2) {
      lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN);
    }
    if (pos <= NUM_ALARM_3) {
      lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (activeCount == 0) {
    alarmsMuted = false;
  }
}

void show_alarm_detail_from_slot(int slot) {
  if (slot < 0 || slot >= MAX_ALARM_DISPLAY)
    return;

  int idx = alarmSlotToIndex[slot];
  if (idx < 0)
    return; // no assigned alarm in this slot

  // Change to the "View details" tab (second tab => index 1)
  lv_tabview_set_act(ui_AlarmsTabview, 1, LV_ANIM_ON);

  // Set the description of the selected alarm
  lv_label_set_text(ui_AlarmDetailLabel, alarmList[idx].description);
}

void Alarm1Cont_cb(lv_event_t *e) { show_alarm_detail_from_slot(0); }
void Alarm2Cont_cb(lv_event_t *e) { show_alarm_detail_from_slot(1); }
void Alarm3Cont_cb(lv_event_t *e) { show_alarm_detail_from_slot(2); }
void Alarm4Cont_cb(lv_event_t *e) { show_alarm_detail_from_slot(3); }

void AlarmButton_cb(lv_event_t *e) {
  // Change to the "Alarms" tab (first tab => index 0)
  lv_tabview_set_act(ui_AlarmsTabview, 0, LV_ANIM_ON);
}

void AlarmsTabview_cb(lv_event_t *e) {
  lv_obj_t *tv = lv_event_get_target(e);
  uint16_t act = lv_tabview_get_tab_act(tv);

  // If we are on tab 0 ("Alarms"), clear the text area
  if (act == 0) {
    lv_label_set_text(ui_AlarmDetailLabel, "");
  }
}

void chart_add_hum_value(float hum) {
  if (humSeries == NULL)
    return;

  // Ensure limits 0–100 %
  if (hum < 0)
    hum = 0;
  if (hum > 100)
    hum = 100;

  lv_chart_set_next_value(ui_HumChart, humSeries, (lv_coord_t)hum);
}

void applyHMIData() {
  // -----------------------------
  // Update numeric values
  // -----------------------------

  airTempValueDetected = ctrl_tel_msg.detectedAirTemperature;
  skinTempValueDetected = ctrl_tel_msg.detectedSkinTemperature;
  humValueDetected = (int)ctrl_tel_msg.detectedHumidity;
  update_labels();

  // Add to charts
  // chart_add_hum_value((float)humValueDetected);
  // chart_add_skin_temp_value((float)skinTempValueDetected);
  if (tempSwitched) {
    chart_add_air_temp((float)airTempValueDetected);
    chart_add_skin_temp((float)skinTempValueDetected);
  }
  chart_add_hum_value((float)humValueDetected);
}

static void AlarmSound_Update() {
  if (alarmActive && !alarmsMuted)
    buzzerOn();
  else
    buzzerOff();
}

static void MuteAlarm_cb(lv_event_t *e) {
  (void)e;
  alarmsMuted = true;

  hmi_msg.muteAlarm = 1;
  hmi_msg.shouldSendData = true;

  lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);

  AlarmSound_Update(); // turns off buzzer if needed
}

void processReceivedAlarm(const ControlBoard_Message_Alarm &alarm) {
  // A) Search by ID
  int idxById = -1;
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarmList[i].id == alarm.id) {
      idxById = i;
      break;
    }
  }

  bool wasKnown = (idxById != -1);
  int index = idxById;

  // B) If not found, search for an empty slot
  if (index == -1) {
    for (int i = 0; i < MAX_ALARMS; i++) {
      if (alarmList[i].state == false) {
        index = i;
        break;
      }
    }
  }
  if (index == -1) {
    log_w("No space to store new alarm ID=%d", alarm.id);
    return;
  }

  bool prevState = wasKnown ? alarmList[index].state : false;

  // "“New alarm" (false->true)
  bool isNewAlarm = (!wasKnown) || (!prevState && alarm.state);

  // Save/update data
  alarmList[index].id = alarm.id;
  strncpy(alarmList[index].type, alarm.type, ALARM_TYPE_LEN);
  alarmList[index].type[ALARM_TYPE_LEN - 1] = '\0';
  strncpy(alarmList[index].description, alarm.description, ALARM_DESC_LEN);
  alarmList[index].description[ALARM_DESC_LEN - 1] = '\0';
  alarmList[index].state = alarm.state;

  // If new alarm activated, unmute alarms
  if (isNewAlarm && alarm.state) {
    alarmsMuted = false;

    // If the remote controller needs to know that it should "unmute":
    hmi_msg.muteAlarm = 0;
    // If your HMI is the one sending that order:
    // hmi_msg.shouldSendData = true;
  }

  update_alarm_panels();
  AlarmSound_Update();
}

static void show_targets_for_mode(void) {
  // In lockScreen: show only the relevant target containers
  lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);

  // For security, hide both and then show the correct one
  lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);

  if (tempSwitched) {
    if (selectedPanel == AIR_PANEL_SELECTED) {
      lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
    } else if (selectedPanel == SKIN_PANEL_SELECTED) {
      lv_obj_clear_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
    }
  }
  // Show humidity target only if humidity switch is active
  if (switchHum) {
    lv_obj_clear_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
  }
}

static void show_unlock_only(void) {
  // Touch anywhere: show only unlock container
  lv_obj_clear_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
}

// Timer callback: update arc value according to elapsed time
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
    // Completed: stop timer and trigger screen change
    if (lockProgressTimer) {
      lv_timer_del(lockProgressTimer);
      lockProgressTimer = NULL;
    }
    // Change to screen 1, mark unlocked and hide arc/unlock container
    lv_scr_load(ui_ScreenMain);
    locked = false;
    if (lockProgressArc)
      lv_obj_add_flag(lockProgressArc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  }
}

// Start the lock progress spinner and timer
static void start_lock_progress(void) {
  lockProgressArc = ui_Spinner1; // use the arc created by SquareLine
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

// Stop and reset the lock progress spinner and timer
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
  // If locked already, do nothing (stay in lock screen)
  if (lv_scr_act() == ui_ScreenLock) {
    stop_lock_progress();
    locked = true;
    show_targets_for_mode();
    return;
  }

  // In lock screen: show only the relevant target containers
  stop_lock_progress();
  locked = true;

  lv_scr_load(ui_ScreenLock);

  // Important: apply visibility logic NOW in ScreenLock
  show_targets_for_mode();

  // Reset inactivity timer of LVGL (optional but recommended)
  lv_disp_trig_activity(NULL);
}

void ImgButton1_Lock_cb(lv_event_t *e) {
  (void)e;
  // Only if we are in main screen, go to lock screen
  if (lv_scr_act() == ui_ScreenMain) {
    enter_lock_screen();
  }
}

// Any touch on the lock screen should open the unlock container
void LockScreenAnyTouch_cb(lv_event_t *e) {
  if (lv_scr_act() != ui_ScreenLock)
    return;

  // If the touch comes from UnlockCont (or any child), DO NOT toggle here
  lv_obj_t *origin =
      lv_event_get_target(e); // original object that received the event
  if (origin != ui_ScreenLock)
    return;

  // Toggle: if Unlock is hidden => show Unlock; if visible => go back to
  // targets
  bool unlockVisible = !lv_obj_has_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);

  if (!unlockVisible) {
    // targets -> unlock
    show_unlock_only();
    locked = false; // (It means we are in unlock state)
  } else {
    // unlock -> targets
    stop_lock_progress(); // in case it was pressed or halfway
    show_targets_for_mode();
    locked = true;
  }
}

// Event callback for pressing the Unlock container
static void UnlockCont_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    // start progress and show spinner
    start_lock_progress();
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    // stop/reset progress and hide spinner
    stop_lock_progress();
  }
}

static void add_unlock_press_cb_recursive(lv_obj_t *obj) {
  if (!obj)
    return;

  // So that the object can generate PRESSED/RELEASED
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj, UnlockCont_event_cb, LV_EVENT_ALL, NULL);

  // Recursively add to children
  uint32_t n = lv_obj_get_child_cnt(obj);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t *child = lv_obj_get_child(obj, i);
    add_unlock_press_cb_recursive(child);
  }
}

void inactivity_timer_cb(lv_timer_t *timer) {

  if (alarmActive) {
    // Maintains active the timer
    lv_disp_trig_activity(NULL);
    return;
  }
  uint32_t inactive = lv_disp_get_inactive_time(NULL);

  if (inactive > INACTIVITY_TIMEOUT_MS) {
    if (lv_scr_act() != ui_ScreenLock) {

      // In lock screen: show only the relevant target containers
      stop_lock_progress();
      locked = true;

      lv_scr_load(ui_ScreenLock);
      show_targets_for_mode(); // <-- This shows the correct targets according
                               // to mode
    }
  }
}

// Callback of button to connect to WiFi
void WifiConnectButton_cb(lv_event_t *e) {
  Serial.print("Connecting to SSID: ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_pass);
}

// ====================================================================
// STATE COMMUNICATION SETUP
// ====================================================================

// ---- NEW: helper to set UI switches without triggering logic ----
static void ui_set_switch_state_silent(lv_obj_t *sw, bool on) {
  if (!sw)
    return;
  if (on)
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  else
    lv_obj_clear_state(sw, LV_STATE_CHECKED);
}

// ---- NEW: apply CTRL,STATE to display (UI + variables) ----
static void Display_ApplyCtrlState(const ControlBoard_Message_State &st) {

  // If the motherboard is not ready yet and sends 0, DO NOT override defaults
  if (st.desiredAirTemperature == 0.0f && st.desiredSkinTemperature == 0.0f &&
      st.desiredHumidity == 0.0f) {
    return;
  }

  // 1) Setpoints/variables base
  airTempValue = st.desiredAirTemperature;
  skinTempValue = st.desiredSkinTemperature;
  humValue = (int)lround(st.desiredHumidity);

  // 2) Actuation flags
  switchTemp = (st.actuation == ACTUATION_TEMPERATURE ||
                st.actuation == ACTUATION_TEMP_AND_HUMIDITY);
  switchHum = (st.actuation == ACTUATION_HUMIDITY ||
               st.actuation == ACTUATION_TEMP_AND_HUMIDITY);
  tempSwitched = switchTemp;
  humSwitched = switchHum;

  // 2.5) Container Panels (ui_Panel1 / ui_Panel3)
  if (switchTemp) {
    lv_obj_set_style_bg_color(ui_Panel1, COLOR_PANEL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);
  } else {
    lv_obj_set_style_bg_color(ui_Panel1, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);
  }

  if (switchHum) {
    lv_obj_set_style_bg_color(ui_Panel3, COLOR_PANEL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);
  } else {
    lv_obj_set_style_bg_color(ui_Panel3, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);
  }

  // 3) Control mode/panel
  if (switchTemp) {

    // --- If SKIN, make sure Skin is visible ---
    if (st.controlMode == CONTROL_SKIN) {
      skinPanelEnabled = true;
      ui_set_switch_state_silent(ui_Switch4, true);
      lv_obj_clear_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(ui_SkinPanelCont, COLOR_PANEL_WHITE,
                                LV_PART_MAIN);
      lv_obj_set_style_opa(ui_SkinPanelCont, LV_OPA_COVER, LV_PART_MAIN);
    }

    if (st.controlMode == CONTROL_AIR) {
      selectedPanel = AIR_PANEL_SELECTED;
      lastSelectedPanel = selectedPanel;
      set_active_panel(ui_AirPanel, ui_SkinPanel);
      lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
    } else {
      selectedPanel = SKIN_PANEL_SELECTED;
      lastSelectedPanel = selectedPanel;
      set_active_panel(ui_SkinPanel, ui_AirPanel);
      lv_obj_clear_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
    }

    arrowsActive = true;
    // Enables click = ADD flag clickable
    lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_WHITE,
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_WHITE, LV_PART_MAIN);
  } else {
    selectedPanel = NO_PANEL_SELECTED;
    arrowsActive = false;
    // disable click = CLEAR flag clickable
    lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
  }

  // 4) Hum arrows
  if (switchHum) {
    // Enable click
    lv_obj_add_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_WHITE, LV_PART_MAIN);
  } else {
    // Disable click
    lv_obj_clear_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
  }

  // 5) Phototherapy
  hmi_msg.phototherapyMode = st.phototherapyMode;
  ui_set_switch_state_silent(ui_Switch3,
                             (st.phototherapyMode == PHOTOTHERAPY_ON));

  // 6) switches 1/2 visual
  ui_set_switch_state_silent(ui_Switch1, switchTemp);
  ui_set_switch_state_silent(ui_Switch2, switchHum);

  // 7) Internal HMI message update without triggering send
  hmi_msg.actuation = st.actuation;
  hmi_msg.controlMode = st.controlMode;
  hmi_msg.desiredAirTemperature = airTempValue;
  hmi_msg.desiredSkinTemperature = skinTempValue;
  hmi_msg.desiredHumidity = humValue;
  hmi_msg.muteAlarm = st.muteAlarm;
  hmi_msg.shouldSendData = false;

  update_labels();
}

static void Display_StateSync_Service(void) {
  if (g_stateSynced)
    return;

  // retry every 500 ms until receiving CTRL,STATE
  uint32_t now = millis();
  if (now - g_lastStateReqMs >= 500) {
    Communication_RequestState();
    g_lastStateReqMs = now;
  }

  if (ctrl_state_msg.newState) {
    ctrl_state_msg.newState = false;
    Display_ApplyCtrlState(ctrl_state_msg);
    g_stateSynced = true;
  }
}

// ====================================================================

void OTA_WIFI_Task(void *pvParameters) {
  wifiInit();
  WIFI_TB_Init();
  for (;;) {
    WifiOTAHandler();
    vTaskDelay(pdMS_TO_TICKS(OTA_TASK_PERIOD_MS));
  }
}

void setup() {

  // ===========================
  // Serial communication and pins initialization
  // ===========================
  Serial.begin(SERIAL_BAUD); // Start serial port for debugging
  initEEPROM();
  pinMode(LED_PIN, OUTPUT);   // Set LED pin as output
  digitalWrite(LED_PIN, LOW); // Initialize LED OFF

  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN); // Initialize I2C on touch pins
  /*Wire1.begin(PIN_D0, PIN_D1);          // Initialize I2C on alternate pins
  (originally commented)

  buzzerOn();
  delay(DELAY_BUZZ_MS);
  buzzerOff();*/

  ESP_LOGI(TAG, "Creating OTA task ...");
  while (xTaskCreatePinnedToCore(OTA_WIFI_Task, "OTA", 8192, NULL,
                                 OTA_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  ESP_LOGI(TAG, "OTA task successfully created!");

  // ===========================
  // Display initialization
  // ===========================
  lcd.begin();               // Initialize the display
  lcd.fillScreen(TFT_BLACK); // Clear the screen with black color
  lcd.setTextSize(2);        // Default text size
  delay(DELAY_SHORT_MS);     // Short pause to stabilize

  lv_init();                      // Initialize LVGL library
  ts.begin();                     // Initialize touchscreen
  ts.setRotation(TOUCH_ROTATION); // Set touchscreen orientation
  lcd.setRotation(LCD_ROTATION);  // Set display rotation

  Communication_Init();

  // ===========================
  // LVGL draw buffer configuration
  // ===========================
  screenWidth = lcd.width();
  screenHeight = lcd.height();
  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL,
                        screenWidth * screenHeight / COLOR_DIVISOR);

  // ===========================
  // LVGL display driver configuration
  // ===========================
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;    // Horizontal resolution
  disp_drv.ver_res = screenHeight;   // Vertical resolution
  disp_drv.flush_cb = my_disp_flush; // Function to send pixels to the display
  disp_drv.draw_buf = &draw_buf;     // Draw buffer
  lv_disp_drv_register(&disp_drv);   // Register the driver

  // ===========================
  // Input device (touch) initialization
  // ===========================

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER; // Pointer type (touch)
  indev_drv.read_cb = my_touchpad_read;   // Callback to read touch input
  indev_drv.long_press_time =
      LOCK_PROGRESS_DURATION_MS;     // ms for LV_EVENT_LONG_PRESSED
  lv_indev_drv_register(&indev_drv); // Register input device in LVGL

  // ===========================
  // Display backlight configuration
  // ===========================
#ifdef TFT_BL
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION); // Setup PWM channel
  ledcAttachPin(TFT_BL, PWM_CHANNEL);     // Attach backlight pin to PWM channel
  ledcWrite(PWM_CHANNEL, BRIGHTNESS_MAX); // Brightness max
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);
  delay(DELAY_BACKLIGHT_MS);
  digitalWrite(TFT_BL, HIGH);
#endif

  // ===========================
  // UI initialization
  // ===========================
  ui_init(); // Initialize UI objects

  UI_ApplyLanguage(LANG_EN); // Default language: English
  lv_dropdown_set_selected(ui_LanguagesDropDown, 1); // English

  // Timer one-shot: 5000 ms -> ScreenMain
  intro_timer = lv_timer_create(intro_timer_cb, 5000, NULL);
  lv_timer_set_repeat_count(intro_timer, 1);

  // State communication: initial request
  Communication_RequestState();
  g_lastStateReqMs = millis();

  // Visual elements initial configuration
  // Bars ranges
  lv_bar_set_range(ui_AirTempBar, 0, 20);
  lv_bar_set_range(ui_SkinTempBar, 0, 20);

  // Humidity remains in 0..100
  lv_bar_set_range(ui_HumBar, 0, 100);

  // Mute alarm button callback:
  lv_obj_add_event_cb(ui_MuteAlarm, MuteAlarm_cb, LV_EVENT_CLICKED, NULL);

  // --- SKIN PANEL: hide at startup ---
  lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);

  // --- Settings containers: hide at startup ---
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  // --- KEYBOARD: hide at startup ---
  lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(ui_Keyboard1, NULL);

  // --- TextArea events for Wifi ---
  lv_obj_add_event_cb(ui_TextArea1, TextArea_focus_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_TextArea2, TextArea_focus_cb, LV_EVENT_CLICKED, NULL);
  // --- KEYBOARD ---
  lv_obj_add_event_cb(ui_Keyboard1, Keyboard_cb, LV_EVENT_ALL, NULL);
  // Register Wifi button callback
  lv_obj_add_event_cb(ui_WifiButton, WifiButton_cb, LV_EVENT_CLICKED, NULL);

  // Register Language button callback
  lv_obj_add_event_cb(ui_LanguagesButton, LanguageButton_cb, LV_EVENT_CLICKED,
                      NULL);

  // ===========================
  // Initialize panel colors
  // ===========================
  lv_obj_set_style_bg_color(ui_Panel2, COLOR_PANEL_WHITE,
                            LV_PART_MAIN); // white
  lv_obj_set_style_opa(ui_Panel2, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_Panel5, COLOR_PANEL_WHITE,
                            LV_PART_MAIN); // white
  lv_obj_set_style_opa(ui_Panel5, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_Panel6, COLOR_PANEL_WHITE,
                            LV_PART_MAIN); // white
  lv_obj_set_style_opa(ui_Panel6, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_Panel4, COLOR_PANEL_WHITE,
                            LV_PART_MAIN); // white
  lv_obj_set_style_opa(ui_Panel4, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_Panel1, COLOR_PANEL_GRAY, LV_PART_MAIN); // grey
  lv_obj_set_style_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_Panel3, COLOR_PANEL_GRAY, LV_PART_MAIN); // grey
  lv_obj_set_style_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_GRAY,
                            LV_PART_MAIN); // temp arrow panel grey
  lv_obj_set_style_opa(ui_ArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_GRAY,
                            LV_PART_MAIN); // temp arrow panel grey
  lv_obj_set_style_opa(ui_ArrowUpTemp, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_GRAY,
                            LV_PART_MAIN); // hum arrow panel grey
  lv_obj_set_style_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_GRAY,
                            LV_PART_MAIN); // hum arrow panel grey
  lv_obj_set_style_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY,
                            LV_PART_MAIN); // Air panel grey
  lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY,
                            LV_PART_MAIN); // Skin panel grey
  lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

  // hide alarm panels at startup
  lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN);

  // Add clickability and callbacks to alarm containers
  lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_event_cb(ui_Alarm1Cont, Alarm1Cont_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_Alarm2Cont, Alarm2Cont_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_Alarm3Cont, Alarm3Cont_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_Alarm4Cont, Alarm4Cont_cb, LV_EVENT_CLICKED, NULL);

  // Make alarm labels clickable too
  lv_obj_add_flag(ui_Alarm1Label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm1Panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui_Alarm1Label, Alarm1Cont_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_Alarm1Panel, Alarm1Cont_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_add_flag(ui_Alarm2Label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm2Panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui_Alarm2Label, Alarm2Cont_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_Alarm2Panel, Alarm2Cont_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_add_flag(ui_Alarm3Label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm3Panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui_Alarm3Label, Alarm3Cont_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_Alarm3Panel, Alarm3Cont_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_add_flag(ui_Alarm4Panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm4Label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui_Alarm4Label, Alarm4Cont_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_Alarm4Panel, Alarm4Cont_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_add_event_cb(ui_ImgButton7, AlarmsTabview_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_AlarmButton, AlarmButton_cb, LV_EVENT_CLICKED, NULL);

  // --- Adjust Alarm description Label ---
  lv_obj_set_width(ui_AlarmDetailLabel, lv_pct(100)); // width 100% of parent
  lv_label_set_long_mode(ui_AlarmDetailLabel,
                         LV_LABEL_LONG_WRAP); // allow line breaks
  lv_obj_set_style_text_align(ui_AlarmDetailLabel, LV_TEXT_ALIGN_CENTER,
                              0); // text centered within the label
  // Optional: position the label in the tab (top center with a small Y margin)
  lv_obj_align(ui_AlarmDetailLabel, LV_ALIGN_TOP_MID, 0, 20);

  lv_obj_add_event_cb(ui_AlarmsTabview, AlarmsTabview_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);
  // Initially, empty description
  lv_label_set_text(ui_AlarmDetailLabel, "");

  lv_obj_add_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);

  // ===========================
  // Lock screen configuration
  // ===========================

  // Initial state of the lock screen: hide target containers and show Unlock
  lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Spinner1, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_event_cb(ui_ImgButton1, ImgButton1_Lock_cb, LV_EVENT_CLICKED,
                      NULL);

  // Any touch on the lock screen should show the unlock container
  lv_obj_add_event_cb(ui_ScreenLock, LockScreenAnyTouch_cb, LV_EVENT_PRESSED,
                      NULL);

  // Make Unlock container clickable and handle press/release for long-press
  // unlock
  add_unlock_press_cb_recursive(ui_UnlockCont);

  // Hide alarm lock container and show check image at startup
  lv_obj_add_flag(ui_AlarmLockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_CheckImg, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_event_cb(ui_AlarmLockCont, AlarmButton_cb, LV_EVENT_CLICKED, NULL);

  // ==========================================
  // LANGUAGE SELECTION CALLBACK
  // ==========================================
  lv_obj_add_event_cb(ui_LanguagesDropDown, LanguagesDropDown_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);

  // ===========================
  // WIFI configuration
  // ===========================

  // WIFI connect button
  lv_obj_add_event_cb(ui_WifiConnectButton, WifiConnectButton_cb,
                      LV_EVENT_CLICKED, NULL);

  // ============================================================================
  // TempChart configuration (AIR + SKIN)
  // ============================================================================

  airTempSeries = configure_temp_chart(ui_AirTempChart, LV_PALETTE_BLUE);
  skinTempSeries = configure_temp_chart(ui_SkinTempChart, LV_PALETTE_BLUE);

  // === Hidden Charts at Startup ===
  lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
  // Hide entire tabview by default so no tabs appear at startup
  lv_obj_add_flag(ui_TabView1, LV_OBJ_FLAG_HIDDEN);

  // ============================================================================
  // HumChart configuration
  // ============================================================================

  // Remove series created by SquareLine
  lv_chart_series_t *s = lv_chart_get_series_next(ui_HumChart, NULL);
  while (s != NULL) {
    lv_chart_series_t *next = lv_chart_get_series_next(ui_HumChart, s);
    // Workaround for crash on free
    s->x_points = NULL;
    s->x_ext_buf_assigned = 0;
    lv_chart_remove_series(ui_HumChart, s);
    s = next;
  }

  lv_chart_set_type(ui_HumChart, LV_CHART_TYPE_LINE);

  // NNumber of points "compact" (you can put 30, 40, etc.)
  lv_chart_set_point_count(ui_HumChart, 50);

  // Range 0–100 %
  lv_chart_set_range(ui_HumChart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);

  // Series for humidity
  humSeries = lv_chart_add_series(
      ui_HumChart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

  // Hide secondary Y axis
  lv_chart_set_axis_tick(ui_HumChart, LV_CHART_AXIS_SECONDARY_Y, 0, 0, 0, 0,
                         false, 0);

  // Hide X axis (no ticks or labels)
  lv_chart_set_axis_tick(ui_HumChart, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0,
                         false, 0);

  // Initialize all points as empty
  for (int i = 0; i < lv_chart_get_point_count(ui_HumChart); i++) {
    humSeries->y_points[i] = LV_CHART_POINT_NONE;
  }
  lv_chart_refresh(ui_HumChart);

  //=================================================================================================

  lv_timer_handler(); // Process any initial LVGL tasks

  // ===========================
  // Temperature and humidity
  // ===========================
  init_values(); // Assign initial random values and update labels

  lv_obj_add_event_cb(ui_Switch1, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_Switch2, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_Switch3, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_Switch4, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // ===========================
  // Simulate switch ON/OFF via labels
  // ===========================

  // ----- SWITCH 1 (Temperature) -----
  // Label9 = ON  (activates Switch1)
  lv_obj_add_flag(ui_Label9, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      ui_Label9,
      [](lv_event_t *e) {
        lv_obj_add_state(ui_Switch1, LV_STATE_CHECKED); // we turn it ON
        lv_event_send(ui_Switch1, LV_EVENT_VALUE_CHANGED,
                      NULL); // triggers Switch_cb()
      },
      LV_EVENT_CLICKED, NULL);

  // Label15 = OFF (deactivates Switch1)
  lv_obj_add_flag(ui_Label15, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      ui_Label15,
      [](lv_event_t *e) {
        lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED); // we turn it OFF
        lv_event_send(ui_Switch1, LV_EVENT_VALUE_CHANGED,
                      NULL); // triggers Switch_cb()
      },
      LV_EVENT_CLICKED, NULL);

  // ----- SWITCH 2 (Humidity) -----
  // Label13 = ON
  lv_obj_add_flag(ui_Label13, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      ui_Label13,
      [](lv_event_t *e) {
        lv_obj_add_state(ui_Switch2, LV_STATE_CHECKED);
        lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  // Label16 = OFF
  lv_obj_add_flag(ui_Label16, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      ui_Label16,
      [](lv_event_t *e) {
        lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
        lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  // ----- SWITCH 3 (Phototherapy) -----
  // Label10 = ON
  lv_obj_add_flag(ui_Label10, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      ui_Label10,
      [](lv_event_t *e) {
        lv_obj_add_state(ui_Switch3, LV_STATE_CHECKED);
        lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  // Label17 = OFF
  lv_obj_add_flag(ui_Label17, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(
      ui_Label17,
      [](lv_event_t *e) {
        lv_obj_clear_state(ui_Switch3, LV_STATE_CHECKED);
        lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  // Connect panel selection callbacks
  setup_panel_callbacks();
  // Connect callbacks for arrows
  setup_arrow_callbacks();
  setup_arrow_hum_callbacks();

  // ===========================
  // Inactivity timer
  // ===========================
  lv_timer_create(inactivity_timer_cb, 1000, NULL);
}

void loop() {

  /*char DHT_buffer[DHT_BUFFER_SIZE];
  int a = (int)dht20.getTemperature();
  int b = (int)dht20.getHumidity();
  snprintf(DHT_buffer, sizeof(DHT_buffer), "%d", a);
  lv_label_set_text(ui_Label1, DHT_buffer);
  snprintf(DHT_buffer, sizeof(DHT_buffer), "%d", b);
  lv_label_set_text(ui_Label2, DHT_buffer);*/

  lv_timer_handler();
  delay(LOOP_DELAY_MS);

  Display_StateSync_Service();

  if (ReceiveMessageFromOtherESP()) {

    // if new alarm received
    if (ctrl_msg_alarm.id != 0) {
      processReceivedAlarm(ctrl_msg_alarm);

      // Clear structure for next alarm
      ctrl_msg_alarm.id = 0;
      ctrl_msg_alarm.state = false;
    } else if (error == false) {
      applyHMIData();
    }
  }

  if (wifiVisible) {
    // If WiFi is connected, show connected container
    if (WiFi.status() == WL_CONNECTED) {
      lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);

      lv_label_set_text(ui_WifiSSIDLabel, WiFi.SSID().c_str());
    } else {
      lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
    }
  }

  /*if (alarmActive) {
      buzzerOn();
  } else {
      buzzerOff();
  }*/
}
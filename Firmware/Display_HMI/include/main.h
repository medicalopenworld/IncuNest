#ifndef VARIABLES_H
#define VARIABLES_H

// All numeric configuration/constants centralized here
// -----------------------------
// Includes
// -----------------------------
#include "Credentials_public.h"
#include "Wifi_OTA.h"
#include <EEPROM.h>
#include <EEPROM_defines.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <lvgl.h>
#include <stdint.h>

#define FWversion "1.0.0"
#define ENABLE_WIFI_OTA true // enable wifi OTA
extern bool OTA_inprogress;

#define OTA_TASK_PRIORITY 4
#define OTA_TASK_PERIOD_MS 1
#define CORE_MONITOR_FREERTOS 0
#define CORE_ID_FREERTOS 1

#define WIFI_NAME "IncuNest_Display"

// Set to true only on the HMI board
#define IS_HMI true

// -----------------------------
// LANGUAGES
// -----------------------------

typedef enum { LANG_ES = 0, LANG_EN = 1, LANG_FR = 2 } ui_lang_t;
extern ui_lang_t g_lang;
extern double airTempValue, skinTempValue;
extern double airTempValueDetected, skinTempValueDetected;
extern int humValue;
extern int humValueDetected;

typedef struct {
  int serialNumber = 0;
} in3ator_parameters;

extern in3ator_parameters in3;

// -----------------------------
// Communication actuation modes
// -----------------------------

#define ACTUATION_NONE 0
#define ACTUATION_TEMPERATURE 1
#define ACTUATION_HUMIDITY 2
#define ACTUATION_TEMP_AND_HUMIDITY 3

#define CONTROL_SKIN false
#define CONTROL_AIR true
#define CONTROL_DEFAULT CONTROL_AIR

#define PHOTOTHERAPY_OFF false
#define PHOTOTHERAPY_ON true
// -----------------------------
// Display
// -----------------------------
constexpr int DISPLAY_WIDTH = 800;
constexpr int DISPLAY_HEIGHT = 480;
constexpr int COLOR_DIVISOR = 15; // used for draw buffer size
constexpr int AREA_PIXEL_OFFSET =
    1; // used when computing width/height from area.x2 - area.x1 + 1

// -----------------------------
constexpr int PIN_HENABLE = 41;
constexpr int PIN_VSYNC = 40;
constexpr int PIN_HSYNC = 39;
constexpr int PIN_PCLK = 0;

constexpr int TOUCH_SDA_PIN = 19;
constexpr int TOUCH_SCL_PIN = 20;
constexpr int TOUCH_INT_PIN = -1;
constexpr int TOUCH_RST_PIN = -1;

constexpr int TFT_BL_PIN = 2;

// -----------------------------
// Timings / delays (ms)
// -----------------------------
constexpr int DELAY_INTRO_MS = 6000;
constexpr int DELAY_SHORT_MS = 200;
constexpr int DELAY_BACKLIGHT_MS = 500;
constexpr int DELAY_BUZZ_MS = 3000; // used only if buzzer flow is uncommented
constexpr int LOOP_DELAY_MS = 10;

// -----------------------------
// LVGL / PWM
// -----------------------------
constexpr int PWM_CHANNEL = 1;
constexpr int PWM_FREQ = 300;
constexpr int PWM_RESOLUTION = 8;
constexpr int BRIGHTNESS_MAX = 255;

// -----------------------------
// Temperature
// -----------------------------
constexpr double AIR_TEMP_MIN = 20.0;
constexpr double AIR_TEMP_MAX = 36.9;
constexpr double SKIN_TEMP_MIN = 35.0;
constexpr double SKIN_TEMP_MAX = 37.5;
constexpr double TEMP_INCREMENT = 0.2;
constexpr double TEMP_ALARM_THRESHOLD = 37.0;
constexpr double TEMP_DIVISOR = 10.0;

constexpr int NUM_ALARM_0 = 0;
constexpr int NUM_ALARM_1 = 1;
constexpr int NUM_ALARM_2 = 2;
constexpr int NUM_ALARM_3 = 3;

#define TEMP_BAR_MIN 0
#define TEMP_BAR_MAX 40 // ºC
#define HUM_BAR_MIN 0
#define HUM_BAR_MAX 100 // %

// -----------------------------
// Panel selection
// -----------------------------
constexpr int NO_PANEL_SELECTED = 0;
constexpr int AIR_PANEL_SELECTED = 1;
constexpr int SKIN_PANEL_SELECTED = 2;

// -----------------------------
// Humidity
// -----------------------------
constexpr int HUM_MIN = 40;
constexpr int HUM_MAX = 95;
constexpr int HUM_STEP = 5;
constexpr int HUM_ALARM_THRESHOLD = 60;

// -----------------------------
// Random ranges (used with random())
// these ranges are the original integers used in your code
// -----------------------------
constexpr int RAND_AIR_MIN = 0;
constexpr int RAND_AIR_MAX = 0;
constexpr int RAND_SKIN_MIN = 0;
constexpr int RAND_SKIN_MAX = 0;
constexpr int RAND_HUM_MIN = 0;
constexpr int RAND_HUM_MAX = 0;

// -----------------------------
// Progress arc for lock long-press
// -----------------------------

static lv_obj_t *lockProgressArc = NULL;
static lv_timer_t *lockProgressTimer = NULL;
static uint32_t lockProgressStart = 0;
static const uint32_t LOCK_PROGRESS_DURATION_MS = 1500; // 1.5 seconds

// -----------------------------
// Serial
// -----------------------------
constexpr int SERIAL_BAUD = 115200;

// -----------------------------
// Misc sizes / lengths
// -----------------------------
constexpr int BUFFER_SIZE = 10;    // used for label char buffers
constexpr int DHT_BUFFER_SIZE = 6; // used in commented DHT code
constexpr int ALARM_TYPE_LEN = 30;
constexpr int ALARM_DESC_LEN = 100;
constexpr int MAX_ALARMS = 10;
constexpr int MAX_ALARM_DISPLAY = 4;

struct Alarm {
  int id;
  char type[ALARM_TYPE_LEN];
  char description[ALARM_DESC_LEN];
  bool state;
};
extern Alarm alarmList[MAX_ALARMS];

// -----------------------------
// LGFX config values (timings, polarity etc.)
// Keep original numeric values from your cfg
// -----------------------------
constexpr int CFG_FREQ_WRITE = 15000000;

constexpr int CFG_HSYNC_POLARITY = 0;
constexpr int CFG_HSYNC_FRONT_PORCH = 40;
constexpr int CFG_HSYNC_PULSE_WIDTH = 48;
constexpr int CFG_HSYNC_BACK_PORCH = 40;

constexpr int CFG_VSYNC_POLARITY = 0;
constexpr int CFG_VSYNC_FRONT_PORCH = 1;
constexpr int CFG_VSYNC_PULSE_WIDTH = 31;
constexpr int CFG_VSYNC_BACK_PORCH = 13;

constexpr int CFG_PCLK_ACTIVE_NEG = 1;
constexpr int CFG_DE_IDLE_HIGH = 0;
constexpr int CFG_PCLK_IDLE_HIGH = 0;

constexpr int CFG_OFFSET_X = 0;
constexpr int CFG_OFFSET_Y = 0;
constexpr int CFG_MEMORY_WIDTH = DISPLAY_WIDTH;
constexpr int CFG_MEMORY_HEIGHT = DISPLAY_HEIGHT;
constexpr int CFG_PANEL_WIDTH = DISPLAY_WIDTH;
constexpr int CFG_PANEL_HEIGHT = DISPLAY_HEIGHT;

// -----------------------------
// Touch / Display rotations
// -----------------------------
constexpr int TOUCH_ROTATION = 3;
constexpr int LCD_ROTATION = 2;

// -----------------------------
// Colors (RGB components) - used with lv_color_make()
// -----------------------------
constexpr int COLOR_PANEL_WHITE_R = 255;
constexpr int COLOR_PANEL_WHITE_G = 255;
constexpr int COLOR_PANEL_WHITE_B = 255;

constexpr int COLOR_PANEL_GRAY_R = 100;
constexpr int COLOR_PANEL_GRAY_G = 100;
constexpr int COLOR_PANEL_GRAY_B = 100;

// convenience lv_color_t constants (not constexpr function calls but const)
static const lv_color_t COLOR_PANEL_WHITE = lv_color_make(
    COLOR_PANEL_WHITE_R, COLOR_PANEL_WHITE_G, COLOR_PANEL_WHITE_B);
static const lv_color_t COLOR_PANEL_GRAY =
    lv_color_make(COLOR_PANEL_GRAY_R, COLOR_PANEL_GRAY_G, COLOR_PANEL_GRAY_B);

// -----------------------------
// Animation timing
// -----------------------------
constexpr int ANIM_TIME_MS = 500;
constexpr int ANIM_PLAYBACK_MS = 500;

// -----------------------------
// Other small numeric defaults used by LVGL calls etc.
// -----------------------------
constexpr int STYLE_SELECTOR_DEFAULT = 0;

// -----------------------------
// Inactivity timeout (ms)
// -----------------------------
#define INACTIVITY_TIMEOUT_MS 20000 // 20s

#endif
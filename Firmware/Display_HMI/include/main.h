#ifndef VARIABLES_H
#define VARIABLES_H

// All numeric configuration/constants centralized here
// -----------------------------
// Includes
// -----------------------------
#include "Credentials_public.h"
#include "Wifi_OTA.h"
#include "display_config.h"
#include <Preferences.h>
#include "EEPROM_defines.h"
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <lvgl.h>
#include <stdint.h>
#include "control_types.h"
#include "alarm_ids.h"

#define FWversion "2.3.0"
#define ENABLE_WIFI_OTA true // enable wifi OTA
extern bool OTA_inprogress;

#define OTA_TASK_PRIORITY 4
#define OTA_TASK_PERIOD_MS 50
#define OTA_TASK_STACK_SIZE 8192
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
extern bool darkMode;        // Global Dark Mode state
extern bool humidityEnabled;   // Humidity control enabled from Settings
extern bool skinPanelEnabled;  // Skin mode control enabled from Settings
extern double airTempValue, skinTempValue;
extern volatile double airTempValueDetected, skinTempValueDetected;
extern int humValue;
extern volatile int humValueDetected;

typedef struct {
  int serialNumber = 0;
} in3ator_parameters;

extern in3ator_parameters in3;
extern bool g_hmiRestoreState;

// -----------------------------
// Communication actuation modes
// -----------------------------
// ActuationMode enum (ACTUATION_OFF=0, ACTUATION_TEMPERATURE=1,
// ACTUATION_HUMIDITY=2, ACTUATION_TEMP_AND_HUMIDITY=3) is now in
// shared control_types.h.
// Backward-compat alias for existing HMI code that uses ACTUATION_NONE.
#define ACTUATION_NONE ((int)ACTUATION_OFF)

// CommStatus enum (COMM_STATUS_NONE ... COMM_STATUS_WIFI_SERVER) is now
// in shared control_types.h.

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
constexpr int COLOR_DIVISOR = 16; // Reducido para liberar ~48KB de RAM interna
                                  // al WiFi WPA2 (era 8 = 96KB, ahora 48KB)
constexpr int AREA_PIXEL_OFFSET =
    1; // used when computing width/height from area.x2 - area.x1 + 1

// -----------------------------
constexpr int PIN_HENABLE = 41; // DE (Was 40)
constexpr int PIN_VSYNC = 40;   // VSYNC (Was 41)
constexpr int PIN_HSYNC = 39;
constexpr int PIN_PCLK = 42;

constexpr int TOUCH_SDA_PIN = 15;
constexpr int TOUCH_SCL_PIN = 16;
// El reset se maneja vía expansor IO PCA9557
constexpr int TOUCH_INT_PIN = -1;
constexpr int TOUCH_RST_PIN = -1;

// -----------------------------
// Backlight I2C Address — centralizado en display_config.h
// (CrowPanel 7.0 usa STC8H1K28 @ DISPLAY_I2C_ADDR_BL = 0x30)
constexpr int I2C_ADDR_BACKLIGHT = DISPLAY_I2C_ADDR_BL;

constexpr int TFT_BL_PIN = DISPLAY_PIN_BL;

// -----------------------------
// Timings / delays (ms)
// -----------------------------
constexpr int DELAY_INTRO_MS = 60000;
constexpr int DELAY_SHORT_MS = 200;
constexpr int DELAY_BACKLIGHT_MS = 500;
constexpr int DELAY_BUZZ_MS = 3000; // used only if buzzer flow is uncommented
constexpr int LOOP_DELAY_MS = 10;

// -----------------------------
// LVGL / PWM
// -----------------------------
constexpr int UI_TASK_STACK_SIZE = 16384; // 8192 * 2
constexpr int UI_TASK_PRIORITY = 5;

constexpr int PWM_CHANNEL = 1;
constexpr int PWM_FREQ = 300;
constexpr int PWM_RESOLUTION = 8;
constexpr int BRIGHTNESS_MAX = 255;

// -----------------------------
// Temperature
// -----------------------------
constexpr double AIR_TEMP_MIN = 30.0;
constexpr double AIR_TEMP_MAX = 38.5;
constexpr double SKIN_TEMP_MIN = 35.0;
constexpr double SKIN_TEMP_MAX = 37.5;
constexpr double TEMP_INCREMENT = 0.2;
constexpr double TEMP_ALARM_THRESHOLD = 37.0;
constexpr double TEMP_DIVISOR = 10.0;
constexpr double TEMP_LABEL_UPDATE_THRESHOLD =
    0.05; // ºC — min change to refresh label
constexpr double SKIN_PROBE_DETECT_THRESHOLD =
    0.1;                                   // ºC — above this = probe present
constexpr double DEFAULT_AIR_TEMP = 30.0;  // ºC — EEPROM default
constexpr double DEFAULT_SKIN_TEMP = 37.0; // ºC — EEPROM default
constexpr int DEFAULT_HUMIDITY = 50;       // %  — EEPROM default

constexpr int NUM_ALARM_0 = 0;
constexpr int NUM_ALARM_1 = 1;
constexpr int NUM_ALARM_2 = 2;
constexpr int NUM_ALARM_3 = 3;

constexpr double TEMP_BAR_DISPLAY_MIN = 20.0; // ºC — lower bound of thermometer
constexpr double TEMP_BAR_DISPLAY_MAX = 40.0; // ºC — upper bound of thermometer
constexpr int TEMP_BAR_RANGE =
    20; // TEMP_BAR_DISPLAY_MAX - TEMP_BAR_DISPLAY_MIN
constexpr int HUM_BAR_MIN = 0;
constexpr int HUM_BAR_MAX = 100;   // %
constexpr int TEMP_CHART_MIN = 20; // ºC — chart Y-axis minimum
constexpr int TEMP_CHART_MAX = 40; // ºC — chart Y-axis maximum
constexpr int HUM_CHART_MIN = 10;  // %  — chart Y-axis minimum
constexpr int HUM_CHART_MAX = 100; // %  — chart Y-axis maximum

// -----------------------------
// Panel selection
// -----------------------------
constexpr int NO_PANEL_SELECTED = 0;
constexpr int AIR_PANEL_SELECTED = 1;
constexpr int SKIN_PANEL_SELECTED = 2;

// -----------------------------
// Humidity
// -----------------------------
constexpr int HUM_MIN = 20;
constexpr int HUM_MAX = 90;
constexpr int HUM_STEP = 5;
constexpr int HUM_ALARM_THRESHOLD = 60;

// -----------------------------
// Phototherapy
// -----------------------------
constexpr int PHOTO_TIMER_DEFAULT_MINUTES = 30; // initial UI value
constexpr int PHOTO_TIMER_EEPROM_DEFAULT = 240; // 4h — EEPROM factory default
constexpr int PHOTO_TIMER_MIN_MINUTES = 120;    // 2h — lower bound
constexpr int PHOTO_TIMER_MAX_MINUTES = 600;    // 10h — upper bound
constexpr int PHOTO_TIMER_STEP_MINUTES = 20;    // +/- step

// -----------------------------
// Chart safe zones
// -----------------------------
constexpr double AIR_SAFE_ZONE_MIN = 34.0;  // ºC
constexpr double AIR_SAFE_ZONE_MAX = 37.0;  // ºC
constexpr double SKIN_SAFE_ZONE_MIN = 36.0; // ºC
constexpr double SKIN_SAFE_ZONE_MAX = 37.5; // ºC
constexpr double HUM_SAFE_ZONE_MIN = 40.0;  // %
constexpr double HUM_SAFE_ZONE_MAX = 70.0;  // %

// -----------------------------
// History chart
// -----------------------------
constexpr int HISTORY_POINTS_5MIN = 30;
constexpr int HISTORY_POINTS_30MIN = 180;
constexpr int HISTORY_POINTS_1H = 360;
constexpr int HISTORY_POINTS_2H = 720;

// -----------------------------
// Communication
// -----------------------------
constexpr int COMM_BAUD_RATE = 115200;
constexpr int COMM_RX_TIMEOUT_MS = 50;
constexpr int COMM_STATE_SYNC_MS = 500;
constexpr double COMM_TEMP_VALID_THRESHOLD =
    0.1; // received temp > this = valid
constexpr int COMM_TASK_STACK_SIZE = 16384;   // 16 KB — margen para calls LVGL profundas
constexpr int COMM_TASK_PRIORITY = 3;
constexpr int COMM_RX_BUFFER_SIZE = 512;
constexpr int COMM_TASK_LOOP_MS = 10;

// -----------------------------
// Audio
// -----------------------------
constexpr uint8_t AUDIO_VOLUME_MIN = 0;
constexpr uint8_t AUDIO_VOLUME_MAX = 21;
constexpr uint8_t AUDIO_VOLUME_DEFAULT = 15;
constexpr int AUDIO_TASK_STACK_SIZE = 8192;

// -----------------------------
// I2C commands (CrowPanel STC8H1K28)
// -----------------------------
constexpr uint8_t I2C_CMD_BUZZER_ON = 246;
constexpr uint8_t I2C_CMD_BUZZER_OFF = 247;
constexpr uint8_t I2C_CMD_SPEAKER_ON = 248;
constexpr uint8_t I2C_CMD_SPEAKER_OFF = 249;

// -----------------------------
// Display pixel clock bounds (OTA validation)
// -----------------------------
constexpr uint32_t DISPLAY_FREQ_MIN = 12000000;
constexpr uint32_t DISPLAY_FREQ_MAX = 25000000;

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

static lv_timer_t *lockProgressTimer = NULL;
static lv_timer_t *unlockTimeoutTimer = NULL;
static lv_timer_t *lockStopDebounceTimer = NULL;
static uint32_t lockProgressStart = 0;
static const uint32_t LOCK_PROGRESS_DURATION_MS = 1500; // 1.5 seconds
static const uint32_t UNLOCK_TIMEOUT_MS = 5000;         // 5 seconds timeout

// Unlock popup border-fill dimensions
static const lv_coord_t UNLOCK_POPUP_W  = 420;
static const lv_coord_t UNLOCK_POPUP_H  = 300;
static const lv_coord_t UNLOCK_BORDER_T = 10;

// -----------------------------
// Serial
// -----------------------------
constexpr int SERIAL_BAUD = 115200;

// -----------------------------
// Time conversion
// -----------------------------
constexpr int SECONDS_PER_MINUTE = 60;
constexpr int MS_PER_SECOND = 1000;

// -----------------------------
// Startup
// -----------------------------
constexpr int STARTUP_DELAY_MS = 0;

// -----------------------------
// Misc sizes / lengths
// -----------------------------
constexpr int BUFFER_SIZE = 10;    // used for label char buffers
constexpr int DHT_BUFFER_SIZE = 6; // used in commented DHT code
constexpr int ALARM_TYPE_LEN = 30;
constexpr int ALARM_DESC_LEN = 100;

// AlarmId enum (NO_ALARMS, HUMIDITY_ALARM ... POWER_SUPPLY_ALARM, NUM_ALARMS,
// MAX_ALARM_STRING_SIZE=255) is now in shared alarm_ids.h.

constexpr int MAX_ALARMS = NUM_ALARMS;
constexpr int MAX_ALARM_DISPLAY = 4;

struct Alarm {
  int id;
  char type[ALARM_TYPE_LEN];
  char description[ALARM_DESC_LEN];
  bool state;
};
extern Alarm alarmList[MAX_ALARMS];

// Nota: Los timings y pines del display están centralizados en display_config.h
// Las constantes CFG_* han sido eliminadas para evitar duplicidades y
// confusión. Usar directamente las macros DISPLAY_* de display_config.h.

constexpr int CFG_OFFSET_X = 0;
constexpr int CFG_OFFSET_Y = 0;

// -----------------------------
// Touch / Display rotations — controladas por DISPLAY_ROTATE_180
// -----------------------------
#if DISPLAY_ROTATE_180
constexpr int TOUCH_ROTATION = 3;
#else
constexpr int TOUCH_ROTATION = 1;
#endif

constexpr int COLOR_PANEL_WHITE_R = 255;
constexpr int COLOR_PANEL_WHITE_G = 255;
constexpr int COLOR_PANEL_WHITE_B = 255;

constexpr int COLOR_PANEL_GRAY_R = 100;
constexpr int COLOR_PANEL_GRAY_G = 100;
constexpr int COLOR_PANEL_GRAY_B = 100;

constexpr int COLOR_PANEL_LIGHT_GRAY_R = 150;
constexpr int COLOR_PANEL_LIGHT_GRAY_G = 150;
constexpr int COLOR_PANEL_LIGHT_GRAY_B = 150;

// convenience lv_color_t constants (not constexpr function calls but const)
static const lv_color_t COLOR_PANEL_WHITE = lv_color_make(
    COLOR_PANEL_WHITE_R, COLOR_PANEL_WHITE_G, COLOR_PANEL_WHITE_B);
static const lv_color_t COLOR_PANEL_GRAY =
    lv_color_make(COLOR_PANEL_GRAY_R, COLOR_PANEL_GRAY_G, COLOR_PANEL_GRAY_B);
static const lv_color_t COLOR_PANEL_LIGHT_GRAY =
    lv_color_make(COLOR_PANEL_LIGHT_GRAY_R, COLOR_PANEL_LIGHT_GRAY_G,
                  COLOR_PANEL_LIGHT_GRAY_B);

// Dark Mode Colors
static const lv_color_t COLOR_BG_DARK = lv_color_make(30, 30, 30);
static const lv_color_t COLOR_PANEL_DARK = lv_color_make(50, 50, 50);
static const lv_color_t COLOR_TEXT_DARK = lv_color_make(220, 220, 220);

// -----------------------------
// Animation timing
// -----------------------------
constexpr int ANIM_TIME_MS = 500;
constexpr int ANIM_PLAYBACK_MS = 500;
constexpr int ZOOM_NORMAL = 256;  // LVGL 1:1 zoom
constexpr int ZOOM_PRESSED = 280; // ~1.09x press effect

// -----------------------------
// Other small numeric defaults used by LVGL calls etc.
// -----------------------------
constexpr int STYLE_SELECTOR_DEFAULT = 0;

// Inactivity timeout moved to EEPROM_defines.h

#endif
#ifndef _MAIN_H
#define _MAIN_H

#define TINY_GSM_MODEM_SIM800
#define modemSerial Serial2
#define THINGSBOARD_ENABLE_PSRAM 0
#define THINGSBOARD_ENABLE_DYNAMIC 1
// #define THINGSBOARD_ENABLE_STREAM_UTILS 1
#include "ThingsBoard.h"
#include <Arduino.h>
#include <TinyGsmClient.h>

#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
// include libraries
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/semphr.h"
#include <Beastdevices_INA3221.h>
#include <EEPROM.h>
#include <Filters.h>
#include <RotaryEncoder.h>
#include <Wire.h>

#include <AH/Timing/MillisMicrosTimer.hpp>
#undef DEBUG
#include <Filters/Butterworth.hpp>

#include "Adafruit_GFX.h"
#include "Adafruit_SHT4x.h"
#include "BluetoothSerial.h"
#include "CommTask.h"
#include "Credentials_public.h"
#include "ESP32_config.h"
#include "GPRS.h"
#include "PID.h"
#include "SPI.h"
#include "SPO2.h"
#include "SparkFun_SHTC3.h"
#include "TCA9555.h"
#include "Wifi_OTA.h"
#include "board.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "IncuNest_humidifier.h"
#include "nvs_flash.h"
#if CONFIG_IDF_TARGET_ESP32S3
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "usb/vcp.hpp"
#include "usb/vcp_ch34x.hpp"
#endif
#include "BQ25730.h"
#include <SensirionI2cSts3x.h>
#include <TFT_eSPI.h> // Hardware-specific library

#include <Arduino_MQTT_Client.h>
#include <Espressif_MQTT_Client.h>
#include <Espressif_Updater.h>

#define DEFAULT_WIFI_EN ON

#define LOG_MODEM_DATA true
#define LOG_INFORMATION true
#define LOG_ERRORS true
#define LOG_ALARMS true
#define LOG_PULSIOXIMETRY false
#define LOG_DRIVE true // Google Drive upload + MB/HMI crash capture
#define LOG_CHARGER true

// Diagnostic: set to 1 to skip the upload task entirely. Writer keeps rotating
// but every closed window is deleted instead of enqueued. Isolates whether the
// crash originates in TLS/upload (heap corruption) or in the writer/littlefs
// path itself.
#define DRIVE_DISABLE_UPLOAD 0

#define USE_SYSTEM_WITHOUT_ACTUATORS_TEST                                      \
  true // only if previous test was OK and that fail cause is not being able to
       // read current measurements
#define WDT_TIMEOUT 75
#if (HW_NUM >= 14)
#define HEATER_MAX_POWER_AMPS 7.5
#define HEATER_SAFE_POWER_AMPS 6.5
#else
#define HEATER_MAX_POWER_AMPS 12.5
#define HEATER_SAFE_POWER_AMPS 11.5
#endif
#define HEATER_POWER_FACTOR_INCREASE 5
#define HEATER_POWER_FACTOR_DECREASE 5
#if (HW_NUM != 6)
#define CURRENT_STABILIZE_THRESHOLD_RATIO 0.1
#endif

#define FAN_RPM_CONVERSION 13333333
#define FAN_UPDATE_TIME_MIN 1000

#define ALARM_SYSTEM_ENABLED true
#define FAN_MAX_CURRENT_OVERRIDE false
#define SILENCED_ALARM false
#define DEFAULT_SOUND_ALARM true

#define UKRAINE_MODE false
#define SENEGAL_MODE false

#define CORE_MONITOR_FREERTOS 0
#define CORE_ID_FREERTOS 1

#define HOLD_PRESS_TO_GO_TO_SETTINGS 0

#define UI_MENU_OLD false

#define BROWN_OUT_BATTERY_MODE 0
#define BROWN_OUT_NORMAL_MODE 0
#define INIT_I2C_RETRIES 3
#define DEFAULT_I2C_SPEED 10000
#define INIT_CURRENT_SENSOR_RETRIES 3

#define ENABLE_WIFI_OTA true // enable wifi OTA
#define ENABLE_GPRS_OTA true // enable GPRS OTA
#define THINGSBOARD_BUFFER_SIZE 4096
#define THINGSBOARD_FIELDS_AMOUNT 64
#define MAX_MESSAGE_SIZE 1024
#define THINGSBOARD_QOS false
#define TELEMETRIES_DECIMALS 2
#define FIRMWARE_FAILURE_RETRIES 12
#define FIRMWARE_PACKET_SIZE 4096
#define WAIT_FAILED_OTA_CHUNKS 10U * 1000U * 1000U

// User Interface display constants
#define valuePosition 245
#define separatorPosition 240
#define unitPosition 315
#define textFontSize 2 // text default size
#define width_select 20
#define TFT_HEIGHT_HEADING 34
#define TFT_SEPARATOR_HEIGHT 4
#define width_back 50
#define side_gap 0
#define letter_height 26
#define letter_width 14
#define logo 40
#define arrow_height 6
#define arrow_tail 5
#define headint_text_height TFT_HEIGHT_HEADING / 5
#define initialSensorsValue "XX"
#define barThickness 3
#define blinkTimeON 1000 // displayed text ON time
#define blinkTimeOFF 100 // displayed text OFF time
#define time_back_draw 255
#define time_back_wait 255

// pages number in UI. Configuration and information will be displayed depending
// on the page number

#define ACTUATION_TEMPERATURE 1
#define ACTUATION_HUMIDITY 2
#define ACTUATION_TEMP_AND_HUMIDITY 3

typedef enum {
  MAIN_MENU_PAGE = 1,
  ACTUATORS_PROGRESS_PAGE,
  SETTINGS_PAGE,
  CALIBRATION_SENSORS_PAGE,
  FIRST_POINT_CALIBRATION_PAGE,
  SECOND_POINT_CALIBRATION_PAGE,
  AUTO_CALIBRATION_PAGE,
  FINE_TUNE_CALIBRATION_PAGE,
} UI_PAGES;

// Mutex for protecting the shared variable
extern SemaphoreHandle_t GPRS_monitor_mutex;
extern SemaphoreHandle_t log_mutex;

// languages numbers that will be called in language variable
typedef enum {
  SPANISH = 0,
  ENGLISH,
  FRENCH,
  PORTUGUESE,
  NUM_LANGUAGES,

} UI_LANGUAGES;
#define defaultLanguage                                                        \
  ENGLISH // Preset number configuration when booting for first time

typedef enum {
  NTC_BABY_MIN_ERROR = 0,
  NTC_BABY_MAX_ERROR,
  DIG_TEMP_ROOM_MIN_ERROR,
  DIG_TEMP_ROOM_MAX_ERROR,
  DIG_HUM_ROOM_MIN_ERROR,
  DIG_HUM_ROOM_MAX_ERROR,
  DIGITAL_SENSOR_NOTFOUND,
  HEATER_CONSUMPTION_MIN_ERROR,
  FAN_CONSUMPTION_MIN_ERROR,
  PHOTOTHERAPY_CONSUMPTION_MIN_ERROR,
  HUMIDIFIER_CONSUMPTION_MIN_ERROR,
  HEATER_CONSUMPTION_MAX_ERROR,
  FAN_CONSUMPTION_MAX_ERROR,
  PHOTOTHERAPY_CONSUMPTION_MAX_ERROR,
  HUMIDIFIER_CONSUMPTION_MAX_ERROR,
  STANDBY_CONSUMPTION_MAX_ERROR,
  DEFECTIVE_SCREEN,
  DEFECTIVE_BUZZER,
  DEFECTIVE_CURRENT_SENSOR,
  UNCALIBRATED_SENSOR,
} HW_ERROR_ID;

typedef enum {
  NO_ALARMS = 0,
  HUMIDITY_ALARM,
  TEMPERATURE_ALARM,
  AIR_THERMAL_CUTOUT_ALARM,
  SKIN_THERMAL_CUTOUT_ALARM,
  AIR_SENSOR_ISSUE_ALARM,
  SKIN_SENSOR_ISSUE_ALARM,
  FAN_ISSUE_ALARM,
  HEATER_ISSUE_ALARM,
  POWER_SUPPLY_ALARM,
  NUM_ALARMS,
  MAX_ALARM_STRING_SIZE = 255,
} ALARMS_ID;

typedef enum {
  COMM_STATUS_NONE = 0,
  COMM_STATUS_GPRS_ONLY = 1,
  COMM_STATUS_GPRS_SERVER = 2,
  COMM_STATUS_WIFI_ONLY = 3,
  COMM_STATUS_WIFI_SERVER = 4
} COMM_STATUS;

typedef enum {
  EVENT_2G = 0,
  EVENT_WIFI,
  EVENT_SERVER_CONNECTION,
  EVENT_OTA_ONGOING,
} UI_EVENTS_ID;

typedef enum {
  EVENT_2G_UI_POS = 5,
  EVENT_SERVER_CONNECTION_UI_POS = EVENT_2G_UI_POS + 2 * letter_width,
  EVENT_WIFI_UI_POS = EVENT_SERVER_CONNECTION_UI_POS + letter_width,
  EVENT_OTA_ONGOING_UI_POS = EVENT_WIFI_UI_POS + letter_width,
} UI_EVENTS_ID_POS;

#define SN_KEY "SN"
#define HW_NUM_KEY "HW_num"
#define HW_REV_KEY "HW_revision"
#define FW_VERSION_KEY "FW_version"
#define CCID_KEY "CCID"
#define IMEI_KEY "IMEI"
#define APN_KEY "APN"
#define COP_KEY "COP"
#define SYSTEM_RESET_REASON "RST_reason"
#define SYS_CURR_STANDBY_TEST_KEY "SYS_current_stanby_test"
#define HEATER_CURR_TEST_KEY "Heater_current_test"
#define FAN_CURR_TEST_KEY "Fan_current_test"
#define PHOTOTHERAPY_CURR_KEY "Phototherapy_current_test"
#define HUMIDIFIER_CURR_KEY "Humidifier_current_test"
#define DISPLAY_CURR_TEST_KEY "Display_current_test"
#define BUZZER_CURR_TEST_KEY "Buzzer_current_test"
#define HW_TEST_KEY "HW_Test"
#define LOCATION_LONGTITUD_KEY "tri_longitud"
#define LOCATION_LATITUD_KEY "tri_latitud"
#define TRI_ACCURACY_KEY "tri_accuracy"
#define UI_LANGUAGE_KEY "UI_language"
#define SKIN_CAPACITANCE_KEY "Skin_CAP"
#define SKIN_TEMPERATURE_KEY "Skin_temp"
#define AIR_TEMPERATURE_KEY "Air_temp"
#define AIR_TEMPERATURE_REDUNDANT_KEY "Air_temp_redundant"
#define AMBIENT_TEMPERATURE_KEY "Amb_temp"
#define HUMIDITY_ROOM_KEY "Humidity"
#define HUMIDITY_AMBIENT_KEY "Amb_humidity"
#define SYSTEM_CURRENT_KEY "SYS_current"
#define SYSTEM_VOLTAGE_KEY "SYS_voltage"
#define CELL_SIGNAL_QUALITY_KEY "CSQ"
#define HEATER_CURRENT_KEY "Heater_current"
#define FAN_CURRENT_KEY "Fan_current"
#define V5_CURRENT_KEY "V5_current"
#define V5_VOLTAGE_KEY "V5_voltage"
#define BAT_CURRENT_KEY "BAT_current"
#define BAT_VOLTAGE_KEY "BAT_voltage"
#define CONTROL_ACTIVE_KEY "Control_active"
#define CONTROL_MODE_KEY "Control_mode"
#define DESIRED_TEMPERATURE_KEY "Temp_desired"
#define DESIRED_HUMIDITY_ROOM_KEY "Hum_desired"
#define HUMIDIFIER_CURRENT_KEY "Humidifier_current"
#define HUMIDIFIER_VOLTAGE_KEY "Humidifier_voltage"
#define PHOTOTHERAPY_PWM_KEY "PH_PWM"
#define PHOTOTHERAPY_CURRENT_KEY "Phototherapy_current"
#define PHOTOTHERAPY_ACTIVE_KEY "Phototherapy_active"
#define CALIBRATED_SENSOR_KEY "Calibrated_sensor"
#define STANBY_TIME_KEY "Standby_time"
#define CONTROL_ACTIVE_TIME_KEY "Control_active_time"
#define HEATER_ACTIVE_TIME_KEY "Heater_active_time"
#define FAN_ACTIVE_TIME_KEY "Fan_active_time"
#define PHOTHERAPY_ACTIVE_TIME_KEY "Phototherapy_active_time"
#define HUMIDIFIER_ACTIVE_TIME_KEY "Humidifier_active_time"
#define GPRS_CONNECTIVITY_KEY "GPRS_connection"
#define WIFI_CONNECTIVITY_KEY "WIFI_connection"
#define HR1_KEY "HR1"
#define HR1_SQI_KEY "HR1_SQI"
#define HR2_KEY "HR2"
#define HR2_SQI_KEY "HR2_SQI"
#define HR3_KEY "HR3"
#define HR3_SQI_KEY "HR3_SQI"
#define SPO2_KEY "SpO2"
#define SPO2_SQI_KEY "SpO2_SQI"
#define PI_KEY "PI"
#define BABY_WEIGHT_KEY "baby_weight_g"
#define BABY_GEST_AGE_KEY "baby_gest_weeks"
#define BABY_AGE_DAYS_KEY "baby_age_days"
#define HUMIDITY_ALARM_KEY "hum_alarm"
#define TEMPERATURE_ALARM_KEY "temp_alarm"
#define AIR_THERMAL_CUTOUT_ALARM_KEY "air_TC_alarm"
#define SKIN_THERMAL_CUTOUT_ALARM_KEY "skin_TC_alarm"
#define AIR_SENSOR_ISSUE_ALARM_KEY "air_sensor_alarm"
#define SKIN_SENSOR_ISSUE_ALARM_KEY "skin_sensor_alarm"
#define FAN_ISSUE_ALARM_KEY "fan_alarm"
#define HEATER_ISSUE_ALARM_KEY "heater_alarm"
#define POWER_SUPPLY_ALARM_KEY "power_alarm"

#define CALIBRATION_RAW_TEMPERATURE_RANGE_SKIN_KEY "Cal_raw_range_skin_temp"
#define CALIBRATION_RAW_TEMPERATURE_LOW_SKIN_KEY "Cal_raw_low_skin_temp"
#define CALIBRATION_RAW_TEMPERATURE_RANGE_AIR_KEY "Cal_raw_range_air_temp"
#define CALIBRATION_RAW_TEMPERATURE_LOW_AIR_KEY "Cal_raw_low_air_temp"
#define CALIBRATION_REFERENCE_TEMPERATURE_RANGE_KEY "Cal_ref_range_temp"
#define CALIBRATION_REFERENCE_TEMPERATURE_LOW_KEY "Cal_ref_low_temp"
#define CALIBRATION_SKIN_FINETUNE_KEY "Cal_finetune_skin_temp"
#define CALIBRATION_AIR_FINETUNE_KEY "Cal_finetune_air_temp"

// Diagnostic telemetry keys
#define BOOT_COUNT_KEY "boot_count"
#define FREE_HEAP_KEY "free_heap"
#define MIN_FREE_HEAP_KEY "min_free_heap"
#define UPTIME_S_KEY "uptime_s"
#define GPRS_KILL_COUNT_KEY "gprs_kill_count"
#define GPRS_MON_KILL_COUNT_KEY "gprs_mon_kill_count"
#define HMI_BOOT_COUNT_KEY "hmi_boot_count"
#define HMI_LAST_RST_KEY "hmi_last_rst"

extern uint32_t g_bootCount;
extern uint32_t g_gprsKillCount;
extern uint32_t g_monKillCount;
extern int g_hmiBootCount;
extern int g_hmiLastRst;
extern int g_restore_photo_minutes;

#define ANALOGREAD_ADC 0
#define MILLIVOTSREAD_ADC 1

#define ADC_READ_FUNCTION MILLIVOTSREAD_ADC

#define ON true
#define OFF false
#define BASIC_CONTROL false
#define PID_CONTROL true
#define CONTROL_SKIN false
#define CONTROL_AIR true

// Tasks priorities
#define POWER_MANAGEMENT_TASK_PRIORITY 1
#define TIME_TRACK_TASK_PRIORITY 2
#define BACKLIGHT_TASK_PRIORITY 3
#define OTA_TASK_PRIORITY 4
#define GPRS_TAST_PRIORITY 5
#define BUZZER_TASK_PRIORITY 6
#define UI_TASK_PRIORITY 7
#define COMMUNICATION_TASK_PRIORITY 7
#define COMMUNICATION_RECEIVER_PRIORITY 7
#define SENSORS_TASK_PRIORITY 8
#define SPO2_TASK_PRIORITY 8
#define SECURITY_TASK_PRIORITY 9
#define GPRS_MONITOR_TASK_PRIORITY 10

#define PWR_HOLD_MS 3000
#define PWR_OFF_UPDATE_INTERVAL_MS 200
#define POWER_MANAGEMENT_TASK_PERIOD_MS 50

#define GPRS_TASK_PERIOD_MS 1
#define OTA_TASK_PERIOD_MS 50
#define SENSORS_TASK_PERIOD_MS 1
#define SPO2_TASK_PERIOD_MS 1
#define SKIN_SENSOR_UPDATE_PERIOD_MS 200 // in millis
#define ROOM_SENSOR_UPDATE_PERIOD_MS 5000
#define DIGITAL_CURRENT_SENSOR_PERIOD_MS 5
#define BUZZER_TASK_PERIOD_MS 10
#define UI_TASK_PERIOD_MS 10
#define SECURITY_TASK_PERIOD_MS 1
#define COMMUNICATION_TASK_PERIOD_MS 1
#define TIME_TRACK_TASK_PERIOD_MS 100
#define BACKLIGHT_TASK_PERIOD_MS 100
#define FAN_TASK_PERIOD_MS 10
#define LOOP_TASK_PERIOD_MS 1000
#define CALIBRATION_TASK_PERIOD_MS 100
#define GPRS_MONITOR_TASK_PERIOD 5000
#define GPRS_MONITOR_TASK_DELETE 30000

#define DIGITAL_CURRENT_SENSOR_READ_PERIOD_MS 500
#define CURRENT_UPDATE_PERIOD_MS 100 // in millis
#define CURRENT_CHECK_PERIOD_MS 2000
#define VOLTAGE_UPDATE_PERIOD_MS 50 // in millis
#define UI_SENSOR_UPDATE_PERIOD_MS 1000
#define POWER_SUPPLY_CHECK_PERIOD 2000 // 2 secs

// buzzer variables
#define buzzerStandbyPeriod                                                    \
  10000 // in millis, there will be a periodic tone when regulating baby's
        // constants
#define buzzerStandbyTone 500        // in micros, tone freq
#define buzzerAlarmTone 500          // in micros, tone freq
#define buzzerAlarmBeepTime 500      // ms ON and OFF per alarm beep cycle
#define buzzerAlarmBeepCount 500     // total beep toggles (~5 min of alarm)
#define buzzerRotaryEncoderTone 2200 // in micros, tone freq
#define buzzerStandbyToneDuration 50 // in micros, tone freq
#define buzzerSwitchDuration 10      // in micros, tone freq
#define buzzerStandbyToneTimes 1     // in micros, tone freq

// EEPROM variables
#define EEPROM_SIZE 512
#define EEPROM_CHECK_STATUS 0
#define EEPROM_FIRST_TURN_ON 10
#define EEPROM_AUTO_LOCK 20
#define EEPROM_LANGUAGE 30
#define EEPROM_SERIAL_NUMBER 40
#define EEPROM_WIFI_EN 50
#define EEPROM_CONTROL_ACTIVE 60
#define EEPROM_PHOTOTHERAPY_ACTIVE 65
#define EEPROM_CONTROL_MODE 70
#define EEPROM_HEATER_TEST 75
#define EEPROM_DESIRED_CONTROL_TEMPERATURE 80
#define EEPROM_DESIRED_CONTROL_HUMIDITY 90
#define EEPROM_RAW_SKIN_TEMP_LOW_CORRECTION 100
#define EEPROM_RAW_SKIN_TEMP_RANGE_CORRECTION 110
#define EEPROM_WIFI_SSID 115
#define EEPROM_WIFI_PASSWORD 145
#define EEPROM_REFERENCE_TEMP_RANGE 170
#define EEPROM_REFERENCE_TEMP_LOW 180
#define EEPROM_FINE_TUNE_TEMP_SKIN 190
#define EEPROM_FINE_TUNE_TEMP_AIR 194
#define EEPROM_THINGSBOARD_PROVISIONED 200
#define EEPROM_THINGSBOARD_TOKEN 205
#define EEPROM_STANDBY_TIME 226
#define EEPROM_CONTROL_ACTIVE_TIME 230
#define EEPROM_HEATER_ACTIVE_TIME 234
#define EEPROM_FAN_ACTIVE_TIME 238
#define EEPROM_PHOTOTHERAPY_ACTIVE_TIME 242
#define EEPROM_HUMIDIFIER_ACTIVE_TIME 246
#define EEPROM_PANIC_OTA_CHANGE 250
#define EEPROM_FAN_PWM 254
#define EEPROM_HEATER_MAX_AMPS 258
#define EEPROM_SKIN_TEMP_MAX 262
#define EEPROM_AIR_TEMP_MAX 266
#define EEPROM_GPRS_ACT_PERIOD 270
#define EEPROM_GPRS_PHOTO_PERIOD 274
#define EEPROM_GPRS_STBY_PERIOD 278
#define EEPROM_FAN_CTL_PWM 282

#define SKIN_CALIBRATION_CORRECTION_FACTOR 0

// configuration variables
#define SWITCH_DEBOUNCE_TIME_MS 30 // encoder debouncing time
#define timePressToSettings                                                    \
  3000 // in millis, time to press to go to settings window in UI
#define DEBUG_LOOP_PRINT 1000 // in millis,

#define DEFAULT_CONTROL_MODE CONTROL_AIR

#define setupAutoCalibrationPoint 0
#define firstAutoCalibrationPoint 1
#define secondAutoCalibrationPoint 2

// GPRS variables to transmit
#define turnedOn 0     // transmit first turned ON with hardware verification
#define room 1         // transmit room variables
#define aliveRefresh 2 // message to let know that incubator is still ON

// sensor variables
#define defaultCurrentSamples 30
#define defaultTestingSamples 8000
#define Rsense 3000 // 3 microohm as shunt resistor

#define MAIN 0
#define SECUNDARY 1
// I2C addresses
#define MAIN_DIGITAL_CURRENT_SENSOR_I2C_ADDRESS 0x41
#define SECUNDARY_DIGITAL_CURRENT_SENSOR_I2C_ADDRESS 0x40
#define AMBIENT_SENSOR_I2C_ADDRESS 0x44

// calibration menu
typedef enum {
  ROOM_SENSOR_STS3X_MAIN = 0,
  ROOM_SENSOR_STS3X_REDUNDANT,
  ROOM_SENSOR_SHTC3,
  ROOM_SENSOR_POSIBILITIES,
} ROOM_SENSORS;

// calibration menu
typedef enum {
  STS3X_MAIN = 0,
  STS3X_REDUNDANT,
  STS3X_NUM,
} STS3X_SENSORS;

#define ROOM_SENSOR_SHTC3_I2C_ADDRESS 0x70
#define ROOM_SENSOR_STS35_I2C_ADDRESS_MAIN 0x4A
#define ROOM_SENSOR_STS35_I2C_ADDRESS_REDUNDANT 0x4B

// #define system constants
#define HUMIDIFIER_DUTY_CYCLE_MAX                                              \
  95 // maximum humidity cycle in heater to be set
#define HUMIDIFIER_DUTY_CYCLE_MIN                                              \
  0 // minimum humidity cycle in heater to be set

#define stepTemperatureIncrement 0.1 // maximum allowed temperature to be set
#define stepHumidityIncrement 5      // maximum allowed temperature to be set
#define presetHumidity 60            // preset humidity
#define maxHum 90                    // maximum allowed humidity to be set
#define minHum 20                    // minimum allowed humidity to be set

#define SKIN_TEMPERATURE_SET_MIN 35
#define AIR_TEMPERATURE_SET_MIN 30
#define SKIN_TEMPERATURE_SET_MAX 37.5
#define AIR_TEMPERATURE_SET_MAX 38.5

// Encoder variables
#define NUMENCODERS 1 // number of encoders in circuit
#if (HW_NUM == 6)
#define ENCODER_TICKS_DIV 1
#else
#define ENCODER_TICKS_DIV 0
#endif
#define encPulseDebounce 200

#define DEFAULT_AUTOLOCK ON

// Graphic variables
#define ERASE false
#define DRAW true

// graphic text configurations
#define graphicTextOffset 1 // bar pos is counted from 1, but text from 0
#define CENTER true
#define LEFT_MARGIN false

// below are all the different variables positions that will be displayed in
// user interface mainMenu
typedef enum {
  CONTROL_MODE_UI_ROW = 0,
  TEMPERATURE_UI_ROW,
  HUMIDITY_UI_ROW,
  LED_UI_ROW,
  START_UI_ROW,
  SETTINGS_UI_ROW,
} MAIN_MENU_UI;

// settings
typedef enum {
  SERIAL_NUMBER_UI_ROW = 0,
  LANGUAGE_UI_ROW,
  WIFI_EN_UI_ROW,
  CCID_UI_ROW,
  CALIBRATION_UI_ROW,
  DEFAULT_VALUES_UI_ROW,
  HW_TEST_UI_ROW,
} SETTINGS_MENU_UI;

// calibration menu
typedef enum {
  AUTO_CALIB_UI_ROW = 0,
  FINE_TUNE_UI_ROW,
  TWO_POINT_CALIB_UI_ROW,
  RESET_CALIB_UI_ROW,
} CALIBRATION_MENU_UI;

// 2p calibration
#define TEMP_CALIB_UI_ROW 0
#define SET_CALIB_UI_ROW 1

// auto calibration
#define AUTO_CALIB_MESSAGE_UI_ROW 0

// Inverted colour options
// #define BLACK 0xFFFF    // Inverted Black
// #define BLUE 0xFFE0     // Inverted Blue
// #define RED 0x07FF      // Inverted Red
// #define GREEN 0xF81F    // Inverted Green
// #define CYAN 0xF800     // Inverted Cyan
// #define MAGENTA 0x07E0  // Inverted Magenta
// #define YELLOW 0x001F   // Inverted Yellow
// #define WHITE 0x0000    // Inverted White
// #define ORANGE 0x02DF   // Inverted Orange

// colour options
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF
#define ORANGE 0xFD20

#define COLOUR_WARNING_TEXT ORANGE
#define COLOUR_MENU BLACK
#define COLOUR_BAR BLACK
#define COLOUR_MENU_TEXT WHITE
#define COLOUR_SELECTED WHITE
#define COLOUR_CHOSEN BLUE
#define COLOUR_HEADING BLUE
#define COLOUR_ARROW BLACK
#define COLOUR_BATTERY BLACK
#define COLOUR_BATTERY_LEFT BLACK
#define COLOUR_FRAME_BAR WHITE
#define COLOUR_LOADING_BAR RED
#define COLOUR_COMPLETED_BAR GREEN
#define introBackColor WHITE
#define introTextColor BLACK
#define transitionEffect BLACK

#define BACKLIGHT_NO_INTERACTION_TIME                                          \
  12000 // time to decrease backlight display if no user actions

#define INIT_I2C_DELAY 50
#define INIT_ROOM_SENSOR_STS3X_DELAY 100
#define INIT_CURRENT_SENSOR_DELAY 50
#define BACKLIGHT_DELAY 2
#define INIT_TFT_DELAY 300
#define WHILE_LOOP_DELAY 1

#define TIME_TRACK_UPDATE_PERIOD 900000 // 15 minutes

typedef struct {
  int skinSensorCapacitance;
  double temperature[SENSOR_TEMP_QTY];
  double airTemperatureRedundantSensor = 0;
  double humidity[SENSOR_HUM_QTY];
  double desiredControlTemperature = false;
  double desiredControlHumidity = false;
  double fineTuneSkinTemperature = false;
  double fineTuneAirTemperature = false;
  double system_current_standby_test = false;
  double heater_current_test = false;
  double fan_current_test = false;
  double phototherapy_current_test = false;
  double humidifier_current_test = false;
  double display_current_test = false;
  double buzzer_current_test = false;
  bool HW_critical_error = false;
  double HW_test_error_code = false;

  double system_current = false;
  double system_voltage = false;
  double heater_current = false;
  int heaterSafeMAXPWM = HEATER_MAX_PWM;
  double fan_current = false;
  double humidifier_current = false;
  double humidifier_voltage = false;
  double phototherapy_current = false;
  double USB_current = false;
  double USB_voltage = false;
  double BATTERY_current = false;
  double BATTERY_voltage = false;
  int serialNumber = false;
  int resetReason = false;
  bool restoreState = false;
  int actuation = false;

  bool controlMode = DEFAULT_CONTROL_MODE;
  bool temperatureControl = false;
  bool humidityControl = false;
  bool phototherapy = false;
  byte phototherapy_intensity = PWM_MAX_VALUE;

  int fanPWM = FAN_PWM;
  int fanCtlPWM = FAN_CTL_PWM_DEFAULT;
  float heaterMaxPowerAmps = HEATER_MAX_POWER_AMPS;
  float skinTemperatureSetMax = SKIN_TEMPERATURE_SET_MAX;
  float airTemperatureSetMax = AIR_TEMPERATURE_SET_MAX;
  int actuating_gprs_period = 60;
  int phototherapy_gprs_period = 180;
  int standby_gprs_period = 3600;

  bool calibrationError = false;

  long last_check_time = false;
  float standby_time = false;
  float control_active_time = false;
  float heater_active_time = false;
  float fan_active_time = false;
  float phototherapy_active_time = false;
  float humidifier_active_time = false;

  bool alarmsEnabled = true;
  bool alarmToReport[NUM_ALARMS];
  char alarmMessage[MAX_ALARM_STRING_SIZE];
  bool previousAlarmReport;

  float fan_rpm = false;
  bool fanEncoderUpdate = false;
  long fanEncoderPeriod[2] = {false, false};

  byte language;

} IncuNest_parameters;

void logE(String dataString);
void logAlarm(String dataString);
void logI(String dataString);
void logModemData(String dataString);
void logSPO2(String dataString);
void logDrive(String dataString);
void logModemData(String dataString);
long secsToMillis(long timeInMillis);
long minsToMillis(long timeInMillis);
float millisToHours(long timeInMillis);
void initHardware(bool printOutputTest);
void UI_mainMenu();
void userInterfaceHandler(int UI_page);
void updateData();
void buzzerHandler();
void buzzerTone(int beepTimes, int timevTaskDelay, int freq);

void shutBuzzer();

double measureMeanConsumption(bool, int);
double measureStabilizedCurrent(bool sensor, int shunt, float offsetCurrent,
                                float minExpected, float maxExpected,
                                int maxTimeMs, int intervalMs = 200);
float measureMeanVoltage(bool, int);
void WIFI_TB_Init();
void WifiOTAHandler(void);
void securityCheck();
void buzzerConstantTone(int freq);
void drawAlarmMessage(char *alertMessage);
void drawHeading(int UI_page, int UI_serialNumber);
void updateHeadingEvent(byte Event, bool event_status);
char *convertStringToChar(char *arrayInput, String input);
int16_t drawCentreString(char *string, int16_t dX, int16_t poY, int16_t size);
void eraseBar(int UI_menu_rows, int bar_pos);
void UI_updateConnectivityEvents();
void updateBar(int UI_menu_rows, int bar_pos);
void graphics(uint8_t UI_page, uint8_t UI_language, uint8_t UI_print_text,
              uint8_t UI_menu_rows, uint8_t UI_var_0, uint8_t UI_var_1);
int graphicHeight(int position);
int16_t drawFloat(float floatNumber, int16_t decimal, int16_t poX, int16_t poY,
                  int16_t size);
void setTextColor(int16_t colour);
int16_t getBackgroundColor();

void turnFans(bool mode);
void alarmTimerStart();
void timeTrackHandler();

bool ongoingCriticalAlarm();
bool ongoingCriticalWiringAlarm();
void setAlarm(byte alarmID);
void setAlarm(byte alarmID, bool alarmSound);
void resetAlarm(byte alarmID);
int getActiveAlarmCount();

void PIDInit();
void PIDHandler();
void startPID(byte var);
void stopPID(byte var);

bool encoderContinuousPress(int UI_page);

void updateLoadingTemperatureBar(float prev, float actual);
void updateLoadingHumidityBar(float prev, float actual);
void drawSelectedTemperature(float temperatureToDraw,
                             float previousTemperatureDrawn);
void drawUnselectedTemperature(float temperatureToDraw,
                               float previousTemperatureDrawn);
void drawHumidity(int UI_humidity, int UI_previousHumdity);
int16_t drawRightString(char *string, int16_t dX, int16_t poY, int16_t size);
void drawStartMessage(bool UI_enableSet, int UI_menu_rows);
void drawCentreNumber(int n, int x, int i);
void drawRightNumber(int n, int x, int i);
void drawBack();
void drawActuatorsSeparators();
void drawStop();
void drawHelpMessage(byte UI_language);
void printLoadingTemperatureBar(double UI_desiredControlTemperature);
void printLoadingHumidityBar(int UI_desiredControlHumidity);
void blinkGoBackMessage();
bool ongoingAlarms();
byte activeAlarm();
void reestartOngoingAlarms();
int alarmPendingToDisplay();
int alarmPendingToClear();
void clearDisplayedAlarm(byte alarm);
void clearAlarmPendingToClear(byte alarm);
char *alarmIDtoString(byte alarmID);
void resendActiveAlarms();
void checkSetMessage(int UI_page, int UI_menu_rows);

bool updateRoomSensor();
bool updateAmbientSensor();
void updateDisplaySensors();

void UI_settings();
void UI_actuatorsProgress();

void wifiInit(void);
void wifiDisable();

void loaddefaultValues();
void UI_calibration();
void firstPointCalibration();
void fineTuneCalibration();
void autoCalibration();
void resetCalibration();
void recapVariables();
void clearCalibrationValues();
void secondPointCalibration();
void saveCalibrationToEEPROM();
int getYpos(int UI_menu_rows, byte row);
bool back_mode();
void setSensorsGraphicPosition(int UI_page);
void updateDisplayHeader();

void initRoomSensor();
void initAmbientSensor();
void initSkinSensor();
void powerMonitor();
void currentMonitor();
void voltageMonitor();

double roundSignificantDigits(double value, int numberOfDecimals);

void initGPIO();
void initEEPROM();
void drawHardwareErrorMessage(long error, bool criticalError,
                              bool calibrationError);
void initAlarms();
void security_check_reboot_cause();
void IRAM_ATTR encSwitchHandler();
void IRAM_ATTR encoderISR();
void IRAM_ATTR fanEncoderISR();
void backlightHandler();

void fanSpeedHandler();
bool measureSkinSensor();
void loadlogo();

void pinMode(uint8_t GPIO, uint8_t Mode);
bool GPIORead(uint8_t GPIO);
void digitalWrite(uint8_t GPIO, uint8_t Mode);

void basictemperatureControl();

#endif
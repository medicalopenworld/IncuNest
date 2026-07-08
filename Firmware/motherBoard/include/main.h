#ifndef _MAIN_H
#define _MAIN_H

#define TINY_GSM_MODEM_SIM800
#define modemSerial Serial2
#define THINGSBOARD_ENABLE_PSRAM 0
#define THINGSBOARD_ENABLE_DYNAMIC 1
#define THINGSBOARD_ENABLE_STREAM_UTILS 1
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
#include <Preferences.h>
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
#include "control_types.h"
#include "alarm_ids.h"
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
#define LOG_PULSIOXIMETRY true
#define LOG_DRIVE true // Google Drive upload + MB/HMI crash capture
#define LOG_CHARGER true

// Diagnostic: set to 1 to skip the upload task entirely. Writer keeps rotating
// but every closed window is deleted instead of enqueued. Isolates whether the
// crash originates in TLS/upload (heap corruption) or in the writer/littlefs
// path itself.
#define DRIVE_DISABLE_UPLOAD 0

#define USE_SYSTEM_WITHOUT_ACTUATORS_TEST \
  true // only if previous test was OK and that fail cause is not being able to
       // read current measurements
#define WDT_TIMEOUT 75
#if (HW_NUM >= 14 && HW_NUM <= 16)
#define HEATER_MAX_POWER_AMPS 7.5
#else
#define HEATER_MAX_POWER_AMPS 9.5
#endif

#define HEATER_SAFE_MARGIN_AMPS 1

#define HEATER_POWER_FACTOR_INCREASE 3
#define HEATER_POWER_FACTOR_DECREASE 3
// Number of fresh in3.heater_current samples (see lastCurrentMeasurement)
// between each heaterSafeMAXPWM ramp step, instead of a wall-clock period.
#define HEATER_RAMP_SAMPLE_CYCLES 3
// Consecutive failed I2C presence probes of the SECUNDARY current sensor
// (heater/USB/battery chip) before raising HEATER_ISSUE_ALARM for a runtime
// dropout - see currentMonitor() in legacy/sensors.cpp. ~10 x 110ms = 1.1s,
// long enough to ride out a transient EMI glitch without missing a real
// dropout for many cycles.
#define HEATER_SENSOR_DROPOUT_ALARM_CYCLES 10
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

#include "ui_constants.h"

// Mutex for protecting the shared variable
extern SemaphoreHandle_t GPRS_monitor_mutex;
extern SemaphoreHandle_t log_mutex;

// Language enum is now in shared control_types.h (Language enum:
// SPANISH=0, ENGLISH, FRENCH, PORTUGUESE, NUM_LANGUAGES)
#define defaultLanguage \
  ENGLISH // Preset number configuration when booting for first time

typedef enum
{
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
  FAN_RPM_MIN_ERROR,
} HW_ERROR_ID;

// AlarmId enum (NO_ALARMS, HUMIDITY_ALARM ... POWER_SUPPLY_ALARM,
// NUM_ALARMS, MAX_ALARM_STRING_SIZE=255) is now in shared alarm_ids.h.
// CommStatus enum (COMM_STATUS_NONE ... COMM_STATUS_WIFI_SERVER) is now
// in shared control_types.h. Both are included transitively via CommTask.h.

#include "telemetry_keys.h"

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

#include "task_config.h"

#define DIGITAL_CURRENT_SENSOR_READ_PERIOD_MS 500
// 110ms (not 100ms): must exceed one full INA3221 conversion cycle
// (AVG_128 x 140us x 2 (bus+shunt) x 3ch ~= 107.52ms, see initHardware.cpp)
// so every currentMonitor() call reflects a genuinely new conversion, not a
// stale repeat. Matches the interval already used by the boot self-test
// (measureThreeActuatorsParallel(..., 110) in initHardware.cpp).
#define CURRENT_UPDATE_PERIOD_MS 110 // in millis
#define VOLTAGE_UPDATE_PERIOD_MS 50 // in millis
#define UI_SENSOR_UPDATE_PERIOD_MS 1000
#define POWER_SUPPLY_CHECK_PERIOD 2000 // 2 secs

// buzzer variables
#define buzzerStandbyPeriod \
  10000                              // in millis, there will be a periodic tone when regulating baby's
                                     // constants
#define buzzerStandbyTone 500        // in micros, tone freq
#define buzzerAlarmTone 500          // in micros, tone freq
#define buzzerAlarmBeepTime 500      // ms ON and OFF per alarm beep cycle
#define buzzerAlarmBeepCount 500     // total beep toggles (~5 min of alarm)
#define buzzerRotaryEncoderTone 2200 // in micros, tone freq
#define buzzerStandbyToneDuration 50 // in micros, tone freq
#define buzzerSwitchDuration 10      // in micros, tone freq
#define buzzerStandbyToneTimes 1     // in micros, tone freq

#include "preferences_keys.h"

#define SKIN_CALIBRATION_CORRECTION_FACTOR 0

// configuration variables
#define SWITCH_DEBOUNCE_TIME_MS 30 // encoder debouncing time
#define timePressToSettings \
  3000                        // in millis, time to press to go to settings window in UI
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
typedef enum
{
  ROOM_SENSOR_STS3X_MAIN = 0,
  ROOM_SENSOR_STS3X_REDUNDANT,
  ROOM_SENSOR_SHTC3,
  ROOM_SENSOR_POSIBILITIES,
} ROOM_SENSORS;

// calibration menu
typedef enum
{
  STS3X_MAIN = 0,
  STS3X_REDUNDANT,
  STS3X_NUM,
} STS3X_SENSORS;

#define ROOM_SENSOR_SHTC3_I2C_ADDRESS 0x70
#define ROOM_SENSOR_STS35_I2C_ADDRESS_MAIN 0x4A
#define ROOM_SENSOR_STS35_I2C_ADDRESS_REDUNDANT 0x4B

// #define system constants
#define HUMIDIFIER_DUTY_CYCLE_MAX \
  95 // maximum humidity cycle in heater to be set
#define HUMIDIFIER_DUTY_CYCLE_MIN \
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

// Graphic variables
#define ERASE false
#define DRAW true

// graphic text configurations
#define graphicTextOffset 1 // bar pos is counted from 1, but text from 0
#define CENTER true
#define LEFT_MARGIN false

// 2p calibration
#define TEMP_CALIB_UI_ROW 0
#define SET_CALIB_UI_ROW 1

// auto calibration
#define AUTO_CALIB_MESSAGE_UI_ROW 0

#define INIT_I2C_DELAY 50
#define INIT_ROOM_SENSOR_STS3X_DELAY 100
#define INIT_CURRENT_SENSOR_DELAY 50
#define BACKLIGHT_DELAY 2
#define INIT_TFT_DELAY 300
#define WHILE_LOOP_DELAY 1

#define TIME_TRACK_UPDATE_PERIOD 900000 // 15 minutes

typedef struct
{
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
  bool photoFirstRun = true;
  long photoTurnOnTime = 0;

  int fanPwrSupplyPWM = FAN_PWR_SUPPLY_PWM;
  int fanCtlPWM = FAN_CTL_PWM_DEFAULT;
  bool fanHasSpeedFeedback = false;
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
  bool fanCommandedOn = false;

  byte language;

} IncuNest_parameters;

void logE(String dataString);
void logAlarm(String dataString);
void logI(String dataString);
void logCharger(String dataString);
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
                                int maxTimeMs, int intervalMs = 200,
                                int window = 3);
float measureMeanVoltage(bool, int);
void WIFI_TB_Init();
void WifiOTAHandler(void);
void securityCheck();
void buzzerConstantTone(int freq);

void turnFans(bool mode);
void alarmTimerStart();
void timeTrackHandler();

bool ongoingCriticalAlarm();
bool ongoingCriticalWiringAlarm();
bool ongoingFanCriticalAlarm();
void setAlarm(byte alarmID);
void setAlarm(byte alarmID, bool alarmSound);
void resetAlarm(byte alarmID);
int getActiveAlarmCount();

void PIDInit();
void PIDHandler();
void startPID(byte var);
void stopPID(byte var);

bool ongoingAlarms();
byte activeAlarm();
void reestartOngoingAlarms();
int alarmPendingToDisplay();
int alarmPendingToClear();
void clearDisplayedAlarm(byte alarm);
void clearAlarmPendingToClear(byte alarm);
char *alarmIDtoString(byte alarmID);
void resendActiveAlarms();

bool updateRoomSensor();
bool updateAmbientSensor();

void wifiInit(void);
void wifiDisable();

void loaddefaultValues();
void recapVariables();

void initRoomSensor();
void initAmbientSensor();
void initSkinSensor();
void powerMonitor();
void currentMonitor();
void voltageMonitor();

// Incremented only when in3.heater_current is genuinely refreshed
// (legacy/sensors.cpp); used by heaterPowerConsumptionCheck() to detect a
// real new sample instead of currentMonitor() merely having ticked.
extern unsigned long heaterCurrentSampleSeq;

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

void fanSpeedHandler();
bool measureSkinSensor();

void pinMode(uint8_t GPIO, uint8_t Mode);
bool GPIORead(uint8_t GPIO);
void digitalWrite(uint8_t GPIO, uint8_t Mode);

void basictemperatureControl();

#endif
/*
  MIT License

  Copyright (c) 2022 Medical Open World, Pablo Sánchez Bergasa

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/

// pio run -e IncuNest_V15 -t upload ; pio device monitor

// Firmware version and head title of UI screen

#include "main.h"
#include "state/state.h"
#include "DriveUpload.h"
#include "CrashReporter.h"
#include <Preferences.h>

static Preferences diag_prefs;
uint32_t g_bootCount = 0;
uint32_t g_gprsKillCount = 0;
uint32_t g_monKillCount = 0;
int g_hmiBootCount = 0;
int g_hmiLastRst = 0;
int g_restore_photo_minutes = 0;

// Build-flag crash simulator. Add -DCRASH_TEST_MB=1 to platformio.ini
// build_flags to fire a panic after CRASH_TEST_MB_DELAY_S seconds (default 130).
// Remove the flag for production builds.
//   CRASH_TEST_MB=1  → abort() (panic, RST_reason=12)
//   CRASH_TEST_MB=2  → null-pointer LoadProhibited
#ifdef CRASH_TEST_MB
#ifndef CRASH_TEST_MB_DELAY_S
#define CRASH_TEST_MB_DELAY_S 130
#endif
static void CrashTestMBTask(void *pv) {
  for (int i = CRASH_TEST_MB_DELAY_S; i > 0; i--) {
    ESP_LOGW("CRASH_TEST_MB", "MB crash firing in %d s", i);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  ESP_LOGE("CRASH_TEST_MB", "Crashing now (CRASH_TEST_MB=%d)", CRASH_TEST_MB);
  vTaskDelay(pdMS_TO_TICKS(50));
#if CRASH_TEST_MB == 2
  { volatile int *p = (int *)0; *p = 0xDEAD; }
#else
  abort();
#endif
  vTaskDelete(NULL);
}
#endif

#if !CONFIG_IDF_TARGET_ESP32S3
TelemetryMessage ctrl_tel_msg = {0, 0, 0, 0};
HMI_CommandMessage hmi_cmd_msg = {0, 0, 0, 0, 0, 0, 0, false};
#endif
char pendingSSID[64] = "";
char pendingPass[64] = "";
char wifi_ssid[64] = "";
char wifi_pass[64] = "";

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

TwoWire *wire;
TwoWire *wire2 = nullptr; // second I2C bus (HW16: SHTC3 + STS35 on pins 19/20)
MAM_IncuNest_Humidifier in3_hum(DEFAULT_ADDRESS);
// Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
SHTC3 mySHTC3;             // Declare an instance of the SHTC3 class
SensirionI2cSts3x mySTS35[STS3X_NUM];
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
RotaryEncoder encoder(ENC_A, ENC_B, RotaryEncoder::LatchMode::TWO03);
Beastdevices_INA3221 mainDigitalCurrentSensor(INA3221_ADDR41_VCC);
Beastdevices_INA3221 secundaryDigitalCurrentSensor(INA3221_ADDR40_GND);
// BQ25730 gestionado por BQ25730.cpp (chargerPresent definido allí)

bool WIFI_EN = true;
long lastDebugUpdate;
long loopCounts;
int page;

double errorTemperature[SENSOR_TEMP_QTY], temperatureCalibrationPoint;
double ReferenceTemperatureRange, ReferenceTemperatureLow;
double provisionalReferenceTemperatureLow;
float diffSkinTemperature,
    diffAirTemperature; // difference between measured temperature and user
                        // input real temperature
double RawTemperatureLow[SENSOR_TEMP_QTY], RawTemperatureRange[SENSOR_TEMP_QTY];
double provisionalRawTemperatureLow[SENSOR_TEMP_QTY];
double temperatureMax[SENSOR_TEMP_QTY], temperatureMin[SENSOR_TEMP_QTY];
int temperature_array_pos; // temperature sensor number turn to measure
bool humidifierState, humidifierStateChange;
int previousHumidity; // previous sampled humidity
float diffHumidity; // difference between measured humidity and user input real
                    // humidity

byte autoCalibrationProcess;

// Sensor check rate (in ms). Both sensors are checked in same interrupt and
// they have different check rates
byte encoderRate = true;
byte encoderCount = false;

volatile long lastEncPulse;
volatile bool statusEncSwitch;

bool roomSensorPresent[ROOM_SENSOR_POSIBILITIES];
bool ambientSensorPresent = false;
bool digitalCurrentSensorPresent[2];

// room variables

float minDesiredTemp[2] = {
    SKIN_TEMPERATURE_SET_MIN,
    AIR_TEMPERATURE_SET_MIN}; // minimum allowed temperature to be set
float maxDesiredTemp[2] = {
    SKIN_TEMPERATURE_SET_MAX,
    AIR_TEMPERATURE_SET_MAX}; // maximum allowed temperature to be set
int presetTemp[2] = {36, 32}; // preset baby skin temperature

boolean A_set;
boolean B_set;
int encoderpinA = ENC_A;         // pin  encoder A
int encoderpinB = ENC_B;         // pin  encoder B
bool encPulsed, encPulsedBefore; // encoder switch status
bool updateUIData;
volatile int EncMove;                 // moved encoder
volatile int lastEncMove;             // moved last encoder
volatile int EncMoveOrientation = -1; // set to -1 to increase values clockwise
volatile int last_encoder_move;       // moved encoder
long encoder_debounce_time =
    true; // in milliseconds, debounce time in encoder to filter signal bounces
long last_encPulsed; // last time encoder was pulsed

// Text Graphic position variables
int humidityX;
int humidityY;
int temperatureX;
int temperatureY;
int separatorTopYPos, separatorMidYPos, separatorBotYPos;
int ypos;
bool print_text;
int initialSensorPosition = separatorPosition - letter_width;
bool pos_text[8];

bool enableSet;
float temperaturePercentage, temperatureAtStart;
float humidityPercentage, humidityAtStart;
int barWidth, barHeight, tempBarPosX, tempBarPosY, humBarPosX, humBarPosY;
int screenTextColor, screenTextBackgroundColour;

// User Interface display variables
bool selected;
char cstring[128];
char *textToWrite;
char *words[12];
char *helpMessage;
byte bar_pos = true;
byte menu_rows;
byte length;
long lastGraphicSensorsUpdate;
long lastSensorsUpdate;
bool enableSetProcess;
long blinking;
bool state_blink;
bool blinkSetMessageState;
long lastBlinkSetMessage;

long lastSuccesfullSensorUpdate[SENSOR_TEMP_QTY];

long lastSkinAttachedSensorUpdate;
long lastRoomSensorUpdate, lastCurrentSensorUpdate;
bool roomSensorOk = false;

IncuNest_parameters in3;

TaskHandle_t taskHandle =
    NULL; // Handle for the task we want to delete if it hangs
long GPRS_lastMillisTaskClear;
bool TB_connected;

QueueHandle_t sharedSensorQueue;
// Mutex for protecting the shared variable
SemaphoreHandle_t GPRS_monitor_mutex;
SemaphoreHandle_t log_mutex = NULL;

void GPRSMonitorTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(GPRS_monitor_mutex, portMAX_DELAY)) // Lock the mutex
    {
      if (millis() - GPRS_lastMillisTaskClear > GPRS_MONITOR_TASK_DELETE) {
        g_monKillCount++;
        diag_prefs.begin("diag", false);
        diag_prefs.putUInt("mon_kill", g_monKillCount);
        diag_prefs.end();
        {
          const char *m = "[MON] killing GPRS_Task after idle timeout\n";
          crashReporterPut(m, strlen(m));
        }
        vTaskDelete(taskHandle); // Delete the hung task
        // Serial.println("Task deleted. Restarting task...");

        // // Optionally restart the task
        // while (xTaskCreatePinnedToCore(GPRS_Task, (const char *)"GPRS", 8192,
        //                                NULL, GPRS_TAST_PRIORITY, &taskHandle,
        //                                CORE_ID_FREERTOS) != pdPASS)
        //   ;
        // logI("GPRS task successfully created!\n");
        vTaskDelete(NULL); // Delete the monitor task
      }
      if (GPRSIsConnectedToServer() || WIFIIsConnectedToServer()) {
        vTaskDelete(NULL); // Delete the monitor task
      }
      // Unlock the mutex
      xSemaphoreGive(GPRS_monitor_mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(GPRS_MONITOR_TASK_PERIOD));
  }
}

void GPRS_Task(void *pvParameters) {
  xTaskCreatePinnedToCore(GPRSMonitorTask, (const char *)"GPRS_MONITOR", 4096,
                          NULL, GPRS_MONITOR_TASK_PRIORITY, NULL,
                          CORE_MONITOR_FREERTOS);
  initGPRS();
  GPRS_TB_Init();
  for (;;) {
    GPRS_Handler();
    // Modify the shared variable
    GPRS_lastMillisTaskClear = millis();
    // Unlock the mutex
    xSemaphoreGive(GPRS_monitor_mutex);
    vTaskDelay(pdMS_TO_TICKS(GPRS_TASK_PERIOD_MS));
  }
}

BQ25730_Status g_bq_status      = {};
bool           g_bq_status_valid = false;
void sensors_Task(void *pvParameters) {
  for (;;) {
    fanSpeedHandler();
    if (millis() - lastSkinAttachedSensorUpdate >
        SKIN_SENSOR_UPDATE_PERIOD_MS) {
      measureSkinSensor();
      lastSkinAttachedSensorUpdate = millis();
    }
    {
      long roomPeriod = roomSensorOk ? ROOM_SENSOR_UPDATE_PERIOD_MS
                                     : ROOM_SENSOR_RECONNECT_MS;
      if (millis() - lastRoomSensorUpdate > roomPeriod) {
        roomSensorOk = updateRoomSensor();
        updateAmbientSensor();
        lastRoomSensorUpdate = millis();
      }
    }
    if (millis() - lastCurrentSensorUpdate > DIGITAL_CURRENT_SENSOR_PERIOD_MS) {
      powerMonitor();
      lastCurrentSensorUpdate = millis();
    }
    {
      static long lastChargerUpdate = 0;
      static bool prev_ac_present   = false;
      static bool charger_in_float  = false;
      static long ichg_low_since    = 0;
      if (chargerPresent && millis() - lastChargerUpdate > 5000) {
        g_bq_status_valid = charge_status(&g_bq_status);
        lastChargerUpdate = millis();
        // Detecta transición ausente→presente del adaptador y reinicializa el
        // chip: algunos BQ25xxx pierden VINDPM/IIN al re-detectar VBUS, así que
        // reaplicamos toda la config para asegurar carga estable.
        // Al reconectar siempre se vuelve a absorción: la batería pudo haberse
        // descargado parcialmente durante el corte.
        if (g_bq_status_valid && g_bq_status.ac_present && !prev_ac_present) {
          if (LOG_CHARGER) logCharger("[CHG] Adaptador detectado → reinicializando config");
          extern TwoWire *wire;
          init_BQ25730(wire);  // restaura MaxChargeVoltage = 14.4V (absorción)
          charger_in_float = false;
          ichg_low_since   = 0;
        }
        if (g_bq_status_valid) prev_ac_present = g_bq_status.ac_present;

        // ── Transición Absorción → Flotación ─────────────────────────────────
        // Cuando ICHG cae por debajo del umbral de corte durante 60 s seguidos,
        // se cambia la tensión objetivo a flotación para mantener la batería sin
        // seguir gasificándola. El umbral (320 mA ≈ C/22 para 7 Ah) equivale a
        // 2–3 counts ADC; la ventana de 60 s evita falsos disparos por ruido.
        if (g_bq_status_valid && g_bq_status.ac_present && !charger_in_float) {
          if (g_bq_status.ichg_ma < BQ25730_ICHG_TERM_MA) {
            if (ichg_low_since == 0) {
              ichg_low_since = millis();
            } else if (millis() - ichg_low_since >= 60000UL) {
              set_charge_voltage(BQ25730_VCHARGE_FLOAT_MV);
              charger_in_float = true;
              ichg_low_since   = 0;
              if (LOG_CHARGER)
                logCharger("[CHG] Absorcion completa → Flotacion " +
                           String(BQ25730_VCHARGE_FLOAT_MV) + " mV");
            }
          } else {
            ichg_low_since = 0;
          }
        }
      }
    }
    ctrl_tel_msg.detectedAirTemperature =
        in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
    ctrl_tel_msg.detectedSkinTemperature = in3.temperature[SKIN_SENSOR];
    ctrl_tel_msg.detectedHumidity = in3.humidity[ROOM_DIGITAL_HUM_SENSOR];
    vTaskDelay(pdMS_TO_TICKS(SENSORS_TASK_PERIOD_MS));
  }
}

void OTA_WIFI_Task(void *pvParameters) {
  WIFI_TB_Init();
  for (;;) {
    WifiOTAHandler();
    vTaskDelay(pdMS_TO_TICKS(OTA_TASK_PERIOD_MS));
  }
}

void buzzer_Task(void *pvParameters) {
  for (;;) {
    buzzerHandler();
    vTaskDelay(pdMS_TO_TICKS(BUZZER_TASK_PERIOD_MS));
  }
}

void security_Task(void *pvParameters) {
  for (;;) {
    if (ALARM_SYSTEM_ENABLED && in3.alarmsEnabled) {
      securityCheck();
    }
    vTaskDelay(pdMS_TO_TICKS(SECURITY_TASK_PERIOD_MS));
  }
}

void TimeTrack_Task(void *pvParameters) {
  for (;;) {
    timeTrackHandler();
    vTaskDelay(pdMS_TO_TICKS(TIME_TRACK_TASK_PERIOD_MS));
  }
}

void Communication_Receiver(void *pvParameters) {
  for (;;) {
    if (hmi_cmd_msg.newCommand) {
      hmi_cmd_msg.newCommand = false;

      String msg = "HMI CMD -> act=" + String(hmi_cmd_msg.actuation) +
                   " mode=" + String(hmi_cmd_msg.controlMode) +
                   " air=" + String(hmi_cmd_msg.desiredAirTemperature, 1) +
                   " skin=" + String(hmi_cmd_msg.desiredSkinTemperature, 1) +
                   " hum=" + String(hmi_cmd_msg.desiredHumidity, 0) +
                   " photo=" + String(hmi_cmd_msg.phototherapyMode) +
                   " mute=" + String(hmi_cmd_msg.muteAlarm) +
                   " lang=" + String(hmi_cmd_msg.language);

      logI(msg);

      if (hmi_cmd_msg.newBabyData) {
        hmi_cmd_msg.newBabyData = false;
        logI("Auto Air baby data -> weight=" +
             String(hmi_cmd_msg.babyWeightGrams) +
             "g gest=" + String(hmi_cmd_msg.babyGestWeeks) +
             "w ageD=" + String(hmi_cmd_msg.babyAgeDays));
      }
      // Reset the alarm stabilization window (see alarmTimerStart(),
      // security.cpp) only on the OFF->ON transition, matching what the
      // now-removed on-board UI (legacy/UI_actuatorsProgress.cpp) used to do
      // when a user confirmed activation from its menu. The HMI-driven path
      // resends this command on every command cycle even while actuation
      // stays on, so gating on the transition (rather than "actuation != 0")
      // avoids perpetually postponing the window and suppressing alarms.
      bool actuationWasOff = (in3.actuation == ACTUATION_OFF);
      in3.actuation = hmi_cmd_msg.actuation;
      { Preferences p; p.begin(NS_STATE, false); p.putUChar(KEY_ACTUATION, in3.actuation); p.end(); }
      if (actuationWasOff && in3.actuation != ACTUATION_OFF) {
        alarmTimerStart();
      }
      if (in3.controlMode != hmi_cmd_msg.controlMode) {
        in3.controlMode = hmi_cmd_msg.controlMode;
        { Preferences p; p.begin(NS_CFG, false); p.putUChar(KEY_CTRL_MODE, in3.controlMode); p.end(); }
      }

      const bool tempBlocked = ongoingCriticalWiringAlarm();

      switch (in3.actuation) {
      case ACTUATION_TEMPERATURE:
        in3.temperatureControl = !tempBlocked;
        in3.humidityControl = false;
        break;
      case ACTUATION_HUMIDITY:
        in3.temperatureControl = false;
        in3.humidityControl = true;
        break;
      case ACTUATION_TEMP_AND_HUMIDITY:
        in3.temperatureControl = !tempBlocked;
        in3.humidityControl = true;
        break;
      default:
        in3.temperatureControl = false;
        in3.humidityControl = false;
        break;
      }

      if (in3.temperatureControl) {
        if (in3.controlMode) {
          in3.desiredControlTemperature = hmi_cmd_msg.desiredAirTemperature;
          startPID(in3.controlMode);
        } else {
          in3.desiredControlTemperature = hmi_cmd_msg.desiredSkinTemperature;
          startPID(!in3.controlMode);
        }
      } else {
        stopPID(CONTROL_AIR);
        stopPID(!CONTROL_AIR);
        ledcWrite(HEATER_PWM_CHANNEL, false);
      }
      if (in3.humidityControl) {
        in3.desiredControlHumidity = hmi_cmd_msg.desiredHumidity;
        startPID(humidityPID);
      } else {
        stopPID(humidityPID);
        in3_hum.turn(OFF);
      }

      in3.phototherapy = hmi_cmd_msg.phototherapyMode;
      { Preferences p; p.begin(NS_STATE, false); p.putUChar(KEY_PHOTO_ACTIVE, in3.phototherapy); p.end(); }
      if (in3.language != hmi_cmd_msg.language) {
        in3.language = hmi_cmd_msg.language;
        resendActiveAlarms();
      }
      if (in3.phototherapy) {
        if (in3.photoFirstRun) {
          in3.phototherapy_intensity = PWM_MAX_VALUE * PHOTOTHERAPY_INITIAL_PWM_PCT / 100;
          in3.photoFirstRun = false;
        }
        in3.photoTurnOnTime = millis();
      }
      ledcWrite(PHOTOTHERAPY_PWM_CHANNEL,
                in3.phototherapy * in3.phototherapy_intensity);
      turnFans(bool(in3.phototherapy || in3.actuation));

      shutBuzzer();
      buzzerTone(buzzerStandbyToneTimes, buzzerSwitchDuration,
                 buzzerRotaryEncoderTone);
    }

    // Handle Phototherapy timer expiration/calculation constantly
    // This runs even if no HMI is connected, ensuring hardware turns OFF.
    getRemainingPhotoTime();

    // Unconditional: every loop inside gates itself on its own PID mode, and
    // the fan loop must run for phototherapy-only activation too (fan is
    // commanded on with in3.actuation == 0 — gating on actuation left
    // FAN_CTL_PWM_CHANNEL undriven and false-fired FAN_ISSUE_ALARM).
    PIDHandler();
    vTaskDelay(pdMS_TO_TICKS(COMMUNICATION_TASK_PERIOD_MS));
  }
}

#if (HW_NUM >= 16)
void PowerManagement_Task(void *pvParameters) {
  while (GPIORead(ON_OFF_SWITCH)) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  for (;;) {
    if (GPIORead(ON_OFF_SWITCH)) { // button pressed (active HIGH)
      unsigned long pressStart = millis();
      unsigned long lastSend = 0;
      char msg[32];

      // Send initial PWR_OFF message with full hold time
      snprintf(msg, sizeof(msg), "CTRL,PWR_OFF,%d\n", PWR_HOLD_MS);
      CommunicationHost_Send(msg);
      lastSend = millis();

      while (GPIORead(ON_OFF_SWITCH)) {
        unsigned long elapsed = millis() - pressStart;
        if (elapsed >= (unsigned long)PWR_HOLD_MS) {
          CommunicationHost_Send("CTRL,PWR_OFF,0\n");
          logI("[PWR] Long press detected, powering off");
          vTaskDelay(pdMS_TO_TICKS(50)); // allow UART to flush
          digitalWrite(PWR_EN, LOW);
          while (true) {
            vTaskDelay(pdMS_TO_TICKS(100));
          }
        }
        // Send remaining time every PWR_OFF_UPDATE_INTERVAL_MS
        if (millis() - lastSend >= PWR_OFF_UPDATE_INTERVAL_MS) {
          int remaining = PWR_HOLD_MS - (int)elapsed;
          if (remaining < 0)
            remaining = 0;
          snprintf(msg, sizeof(msg), "CTRL,PWR_OFF,%d\n", remaining);
          CommunicationHost_Send(msg);
          lastSend = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      // Button released before timeout — cancel
      CommunicationHost_Send("CTRL,PWR_OFF_CANCEL\n");
    }
    vTaskDelay(pdMS_TO_TICKS(POWER_MANAGEMENT_TASK_PERIOD_MS));
  }
}
#endif

extern "C" int sync_vprintf(const char *fmt, va_list args) {
  // Tee formatted output into the crash-report ring so that, after a panic,
  // the last few KB of ESP_LOG context survive in RTC memory.
  char ring_buf[256];
  va_list args_copy;
  va_copy(args_copy, args);
  int ring_len = vsnprintf(ring_buf, sizeof(ring_buf), fmt, args_copy);
  va_end(args_copy);
  if (ring_len > 0) {
    if (ring_len > (int)sizeof(ring_buf))
      ring_len = sizeof(ring_buf);
    crashReporterPut(ring_buf, ring_len);
  }

  if (log_mutex == NULL) {
    return vprintf(fmt, args);
  }
  if (xSemaphoreTakeRecursive(log_mutex, portMAX_DELAY) == pdTRUE) {
    int res = vprintf(fmt, args);
    xSemaphoreGiveRecursive(log_mutex);
    return res;
  }
  return vprintf(fmt, args);
}

void setup() {
  state_init();

#if (HW_NUM >= 16)
  // Power latch: a single press (button already held when boot starts) is
  // enough to keep the device ON. Latch PWR_EN immediately, then wait for
  // the button to be released so the runtime task starts from a clean state.
  {
    pinMode(PWR_EN, OUTPUT);
    digitalWrite(PWR_EN, HIGH);
  }
#endif
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  debugSerial.begin(115200);
  log_mutex = xSemaphoreCreateRecursiveMutex();
  crashReporterInit();
  esp_log_set_vprintf(sync_vprintf);

  GPRS_monitor_mutex = xSemaphoreCreateBinary();
  security_check_reboot_cause();

  diag_prefs.begin("diag", false);
  g_bootCount = diag_prefs.getUInt("boots", 0) + 1;
  diag_prefs.putUInt("boots", g_bootCount);
  diag_prefs.putInt("last_rst", (int)esp_reset_reason());
  g_gprsKillCount = diag_prefs.getUInt("gprs_kill", 0);
  g_monKillCount  = diag_prefs.getUInt("mon_kill", 0);
  diag_prefs.end();
  logI("[DIAG] bootCount=" + String(g_bootCount) +
       " lastRst="    + String((int)esp_reset_reason()) +
       " gprs_kill="  + String(g_gprsKillCount) +
       " mon_kill="   + String(g_monKillCount));

  initEEPROM();
  initGPIO();

  // Set the serial number and log it
  ctrl_tel_msg.serialNumber = in3.serialNumber;
  logI("IncuNest debug uart, version v" + String(FWversion) + "/" +
       String(HWversion) + ", SN: " + String(in3.serialNumber));

  initHardware(false);
  initDriveUpload();
  crashReporterMaybeFlush();
  // SPO2/AFE must initialize SPI (GPIO19 as MISO) before USB host starts.
  // On V15, usb_host_install() claims GPIO19 as USB D−, which conflicts with
  // AFE SPI MISO. Initializing AFE first ensures the chip is configured and
  // DRDY is active before USB takes GPIO19.
  initSPO2();

#if CONFIG_IDF_TARGET_ESP32S3
  // --- Initialize UART/USB communication between ESP32 boards ---
  logI("Initializing communication task ...");
  CommunicationHost_Init();

  xTaskCreatePinnedToCore(Communication_Task, "COMM_TASK", 4096, NULL,
                          COMMUNICATION_TASK_PRIORITY, NULL,
                          CORE_ID_FREERTOS // o 0/1 según tu placa
  );

  xTaskCreatePinnedToCore(Communication_Receiver, "COMM_TASK_RX", 4096, NULL,
                          COMMUNICATION_RECEIVER_PRIORITY, NULL,
                          CORE_ID_FREERTOS // o 0/1 según tu placa
  );
  logI("Communication task successfully created!\n");
#endif
  if (WIFI_EN) {
    wifiInit();
    logI("WiFi Initialization started.");
  }

#if (HW_NUM >= 16)
  logI("Creating power management task ...\n");
  while (xTaskCreatePinnedToCore(PowerManagement_Task, "PWR_MGMT", 2048, NULL,
                                 POWER_MANAGEMENT_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Power management task successfully created!\n");
#endif

  logI("Creating buzzer task ...\n");
  while (xTaskCreatePinnedToCore(buzzer_Task, "BUZZER", 4096, NULL,
                                 BUZZER_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Buzzer task successfully created!\n");

  logI("Creating sensors task ...\n");
  while (xTaskCreatePinnedToCore(sensors_Task, "SENSORS", 4096, NULL,
                                 SENSORS_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Sensors task successfully created!\n");

  logI("Creating security task ...\n");
  while (xTaskCreatePinnedToCore(security_Task, "SECURITY", 4096, NULL,
                                 SECURITY_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Security task successfully created!\n");

  logI("Creating GPRS task ...\n");
  while (xTaskCreatePinnedToCore(GPRS_Task, "GPRS", 16384, NULL,
                                 GPRS_TAST_PRIORITY, &taskHandle,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("GPRS task successfully created!\n");

  logI("Creating OTA task ...\n");
  while (xTaskCreatePinnedToCore(OTA_WIFI_Task, "OTA", 16384, NULL,
                                 OTA_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("OTA task successfully created!\n");

  logI("Creating time track task ...\n");
  while (xTaskCreatePinnedToCore(TimeTrack_Task, "TimeTrack", 4096, NULL,
                                 TIME_TRACK_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Time track task successfully created!\n");

#ifdef CRASH_TEST_MB
  xTaskCreatePinnedToCore(CrashTestMBTask, "CRASH_TEST_MB", 2048, NULL, 1,
                          NULL, CORE_ID_FREERTOS);
#endif
}

void loop() {
  watchdogReload();
  updateData();
  vTaskDelay(pdMS_TO_TICKS(LOOP_TASK_PERIOD_MS));
}
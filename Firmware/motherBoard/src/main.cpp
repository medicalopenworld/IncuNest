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

// Firmware version and head title of UI screen

#include "main.h"

#include <Arduino.h>

TwoWire *wire;
MAM_in3ator_Humidifier in3_hum(DEFAULT_ADDRESS);
// Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
SHTC3 mySHTC3;             // Declare an instance of the SHTC3 class
SensirionI2cSts3x mySTS35[STS3X_NUM];
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
RotaryEncoder encoder(ENC_A, ENC_B, RotaryEncoder::LatchMode::TWO03);
Beastdevices_INA3221 mainDigitalCurrentSensor(INA3221_ADDR41_VCC);
Beastdevices_INA3221 secundaryDigitalCurrentSensor(INA3221_ADDR40_GND);
BQ25792 charger(0, 0);

bool WIFI_EN = true;
long lastDebugUpdate;
long loopCounts;
int page;

double errorTemperature[SENSOR_TEMP_QTY], temperatureCalibrationPoint;
double ReferenceTemperatureRange, ReferenceTemperatureLow;
double provisionalReferenceTemperatureLow;
double fineTuneSkinTemperature, fineTuneAirTemperature;
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
bool goToSettings = false;
bool autoLock; // setting that enables backlight switch OFF after a given time
               // of no user actions
long lastbacklightHandler; // last time there was a encoder movement or pulse

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

int ScreenBacklightMode;
long lastSkinAttachedSensorUpdate;
long lastRoomSensorUpdate, lastCurrentSensorUpdate;

in3ator_parameters in3;

TaskHandle_t taskHandle =
    NULL; // Handle for the task we want to delete if it hangs
long GPRS_lastMillisTaskClear;
bool TB_connected;

QueueHandle_t sharedSensorQueue;
// Mutex for protecting the shared variable
SemaphoreHandle_t GPRS_monitor_mutex;

void GPRSMonitorTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(GPRS_monitor_mutex, portMAX_DELAY)) // Lock the mutex
    {
      if (millis() - GPRS_lastMillisTaskClear > GPRS_MONITOR_TASK_DELETE) {
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
    if (!WIFIIsConnected()) {
      GPRS_Handler();
    }
    // Modify the shared variable
    GPRS_lastMillisTaskClear = millis();
    // Unlock the mutex
    xSemaphoreGive(GPRS_monitor_mutex);
    vTaskDelay(pdMS_TO_TICKS(GPRS_TASK_PERIOD_MS));
  }
}

void Backlight_Task(void *pvParameters) {
  for (;;) {
    backlightHandler();
    vTaskDelay(pdMS_TO_TICKS(BACKLIGHT_TASK_PERIOD_MS));
  }
}

void sensors_Task(void *pvParameters) {
  int touchValue, touchValueMean, touchValueOK;
  for (;;) {
    fanSpeedHandler();
    measureNTCTemperature();
    if (millis() - lastSkinAttachedSensorUpdate >
        SKIN_CAPACITANCE_UPDATE_PERIOD_MS) {
      lastSkinAttachedSensorUpdate = millis();
      GPIOWrite(TOUCH_SENSOR_SEL, HIGH);
      initPin(TOUCH_SENSOR, INPUT);
      touchValueOK = false;
      touchValueMean = false;
      for (int i = 0; i < TOUCH_MEAN_TIMES; i++) {
        vTaskDelay(pdMS_TO_TICKS(TOUCH_DELAY_BETWEEN_MEASURES_MS));
        touchValue = touchRead(TOUCH_SENSOR);
        touchValueMean += touchValue;
        if (touchValue) {
          touchValueOK++;
        }
      }
      if (touchValueOK) {
        in3.skinSensorCapacitance = touchValueMean / touchValueOK;
      }
      initPin(TOUCH_SENSOR, OUTPUT);
      GPIOWrite(TOUCH_SENSOR_SEL, LOW);
    }
    if (millis() - lastRoomSensorUpdate > ROOM_SENSOR_UPDATE_PERIOD_MS) {
      updateRoomSensor();
      updateAmbientSensor();
      lastRoomSensorUpdate = millis();
    }
    if (millis() - lastCurrentSensorUpdate > DIGITAL_CURRENT_SENSOR_PERIOD_MS) {
      powerMonitor();
      lastCurrentSensorUpdate = millis();
    }
    ctrl_tel_msg.detectedAirTemperature = in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
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

void UI_Task(void *pvParameters) {
  if (in3.restoreState) {
    UI_actuatorsProgress();
  } else {
    if (goToSettings) {
      UI_settings();
    } else {
      UI_mainMenu();
    }
  }
  for (;;) {
    userInterfaceHandler(page);
    vTaskDelay(pdMS_TO_TICKS(UI_TASK_PERIOD_MS));
  }
}

void TimeTrack_Task(void *pvParameters) {
  for (;;) {
    timeTrackHandler();
    vTaskDelay(pdMS_TO_TICKS(TIME_TRACK_TASK_PERIOD_MS));
  }
}

void setup() {
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  debugSerial.begin(115200);
  logI("in3ator debug uart, version v" + String(FWversion) + "/" +
       String(HWversion) + ", SN: " + String(in3.serialNumber));

  // --- Initialize UART communication between ESP32 boards ---
  logI("Initializing communication task ...");
  Communication_Init();
  logI("Communication task successfully created!\n");

  GPRS_monitor_mutex = xSemaphoreCreateBinary();
  security_check_reboot_cause();
  initGPIO();
  initEEPROM();
  initRoomSensor();
  if (!GPIORead(ENC_SWITCH)) {
    goToSettings = true;
  }

  initHardware(false);
  if (WIFI_EN) {
    wifiInit();
  }

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
  while (xTaskCreatePinnedToCore(GPRS_Task, "GPRS", 8192, NULL,
                                 GPRS_TAST_PRIORITY, &taskHandle,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("GPRS task successfully created!\n");

  logI("Creating OTA task ...\n");
  while (xTaskCreatePinnedToCore(OTA_WIFI_Task, "OTA", 8192, NULL,
                                 OTA_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("OTA task successfully created!\n");

  logI("Creating Backlight task ...\n");
  while (xTaskCreatePinnedToCore(Backlight_Task, "BACKLIGHT", 4096, NULL,
                                 BACKLIGHT_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Backlight task successfully created!\n");

  logI("Creating time track task ...\n");
  while (xTaskCreatePinnedToCore(TimeTrack_Task, "TimeTrack", 4096, NULL,
                                 TIME_TRACK_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Time track task successfully created!\n");

#if HW_NUM < 15
  logI("Creating UI task ...\n");
  while (xTaskCreatePinnedToCore(UI_Task, "UI", 4096, NULL, UI_TASK_PRIORITY,
                                 NULL, CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("UI task successfully created!\n");
#endif
}

void loop() {
  watchdogReload();
  updateData();
  vTaskDelay(pdMS_TO_TICKS(LOOP_TASK_PERIOD_MS));
}
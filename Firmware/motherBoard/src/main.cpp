/*
  MIT License

  Copyright (c) 2022 
  Medical Open World, Pablo Sánchez Bergasa

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

// ===========================================================
//  IN3ATOR MOTHERBOARD FIRMWARE
//  ESP32-S3  |  UART Communication with Display ESP32
// ===========================================================

#include "main.h"
#include "communication.h"
#include <Arduino.h>

// ================================
// HARDWARE DECLARATIONS
// ================================
TwoWire *wire;
MAM_in3ator_Humidifier in3_hum(DEFAULT_ADDRESS);
TFT_eSPI tft = TFT_eSPI();
SHTC3 mySHTC3;
SensirionI2cSts3x mySTS35[STS3X_NUM];
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
RotaryEncoder encoder(ENC_A, ENC_B, RotaryEncoder::LatchMode::TWO03);
Beastdevices_INA3221 mainDigitalCurrentSensor(INA3221_ADDR41_VCC);
Beastdevices_INA3221 secundaryDigitalCurrentSensor(INA3221_ADDR40_GND);
BQ25792 charger(0, 0);

// ================================
// GLOBAL VARIABLES
// ================================
bool WIFI_EN = true;
long lastDebugUpdate;
long loopCounts;
int page;

bool goToSettings = false;
bool autoLock;
bool selected;
bool enableSet;
bool ambientSensorPresent = false;
bool roomSensorPresent[ROOM_SENSOR_POSIBILITIES];
bool digitalCurrentSensorPresent[2];

in3ator_parameters in3;
TaskHandle_t taskHandle = NULL;
QueueHandle_t sharedSensorQueue;
SemaphoreHandle_t GPRS_monitor_mutex;


// Tiempos de actualización (por errores de compilacion, si no borrar)
unsigned long GPRS_lastMillisTaskClear = 0;
unsigned long lastSkinAttachedSensorUpdate = 0;
unsigned long lastRoomSensorUpdate = 0;
unsigned long lastCurrentSensorUpdate = 0;

// ================================
// TASKS DECLARATIONS
// ================================
void GPRSMonitorTask(void *pvParameters);
void GPRS_Task(void *pvParameters);
void Backlight_Task(void *pvParameters);
void sensors_Task(void *pvParameters);
void OTA_WIFI_Task(void *pvParameters);
void buzzer_Task(void *pvParameters);
void security_Task(void *pvParameters);
void UI_Task(void *pvParameters);
void TimeTrack_Task(void *pvParameters);

// ================================
// TASK DEFINITIONS
// ================================
void GPRSMonitorTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(GPRS_monitor_mutex, portMAX_DELAY)) {
      if (millis() - GPRS_lastMillisTaskClear > GPRS_MONITOR_TASK_DELETE) {
        vTaskDelete(taskHandle);
        vTaskDelete(NULL);
      }
      if (GPRSIsConnectedToServer() || WIFIIsConnectedToServer()) {
        vTaskDelete(NULL);
      }
      xSemaphoreGive(GPRS_monitor_mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(GPRS_MONITOR_TASK_PERIOD));
  }
}

void GPRS_Task(void *pvParameters) {
  xTaskCreatePinnedToCore(GPRSMonitorTask, "GPRS_MONITOR", 4096,
                          NULL, GPRS_MONITOR_TASK_PRIORITY, NULL,
                          CORE_MONITOR_FREERTOS);
  initGPRS();
  GPRS_TB_Init();
  for (;;) {
    if (!WIFIIsConnected()) {
      GPRS_Handler();
    }
    GPRS_lastMillisTaskClear = millis();
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

    if (millis() - lastSkinAttachedSensorUpdate > SKIN_CAPACITANCE_UPDATE_PERIOD_MS) {
      lastSkinAttachedSensorUpdate = millis();
      GPIOWrite(TOUCH_SENSOR_SEL, HIGH);
      initPin(TOUCH_SENSOR, INPUT);
      touchValueOK = false;
      touchValueMean = false;
      for (int i = 0; i < TOUCH_MEAN_TIMES; i++) {
        vTaskDelay(pdMS_TO_TICKS(TOUCH_DELAY_BETWEEN_MEASURES_MS));
        touchValue = touchRead(TOUCH_SENSOR);
        touchValueMean += touchValue;
        if (touchValue) touchValueOK++;
      }
      if (touchValueOK) in3.skinSensorCapacitance = touchValueMean / touchValueOK;
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
    if (goToSettings) UI_settings();
    else UI_mainMenu();
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

// ================================
// SETUP
// ================================
void setup() {
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  debugSerial.begin(115200);
  logI("in3ator motherboard booting...");

  GPRS_monitor_mutex = xSemaphoreCreateBinary();
  security_check_reboot_cause();
  initGPIO();
  initEEPROM();
  initRoomSensor();

  if (!GPIORead(ENC_SWITCH)) goToSettings = true;

  initHardware(false);
  if (WIFI_EN) wifiInit();

  // 🔹 Inicializar comunicación UART con display
  logI("Initializing UART communication with Display ESP32...");
  initCommunication();
  logI("UART Communication initialized successfully.");

  // 🔹 Crear tareas FreeRTOS principales
  xTaskCreatePinnedToCore(buzzer_Task, "BUZZER", 4096, NULL,
                          BUZZER_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
  xTaskCreatePinnedToCore(sensors_Task, "SENSORS", 4096, NULL,
                          SENSORS_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
  xTaskCreatePinnedToCore(security_Task, "SECURITY", 4096, NULL,
                          SECURITY_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
  xTaskCreatePinnedToCore(GPRS_Task, "GPRS", 8192, NULL,
                          GPRS_TAST_PRIORITY, &taskHandle, CORE_ID_FREERTOS);
  xTaskCreatePinnedToCore(OTA_WIFI_Task, "OTA", 8192, NULL,
                          OTA_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
  xTaskCreatePinnedToCore(Backlight_Task, "BACKLIGHT", 4096, NULL,
                          BACKLIGHT_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
  xTaskCreatePinnedToCore(TimeTrack_Task, "TimeTrack", 4096, NULL,
                          TIME_TRACK_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
#if HW_NUM < 15
  xTaskCreatePinnedToCore(UI_Task, "UI", 4096, NULL,
                          UI_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
#endif
  logI("All tasks created successfully!");
}

// ================================
// LOOP
// ================================
void loop() {
    watchdogReload();
    updateData();

    // 🔹 Comunicación con display
    DisplayCommand cmd;
    DisplayMessage msg;

    if (readDisplayCommand(&cmd)) {
        logI("Display command received:");
        logI("Start: " + String(cmd.startButtonPressed));
        logI("Stop: " + String(cmd.stopButtonPressed));
        logI("Target Temp Air: " + String(cmd.targetTemperatureAir));
        logI("Target Temp Skin: " + String(cmd.targetTemperatureSkin));
        logI("Target Humidity: " + String(cmd.targetHumidity));
    }

    // 🔹 Enviar valores actuales al display
    msg.temperatureAir = 37.1;   // ejemplo real, reemplazar con tu sensor
    msg.temperatureSkin = 36.5;  // ejemplo real, reemplazar con tu sensor
    msg.humidity = 55.2;         // ejemplo real, reemplazar con tu sensor
    msg.alarmActive = false;
    msg.controlMode = true;
    msg.phototherapyOn = false;
    sendDisplayMessage(&msg);

    vTaskDelay(pdMS_TO_TICKS(LOOP_TASK_PERIOD_MS));
}

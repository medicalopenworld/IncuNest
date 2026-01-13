#include "main.h"
#include "UITask.h"
#include "communication.h"
#include "esp_log.h"
#include <PCA9557.h>
#include <lvgl.h>

static const char *TAG = "Main";

bool OTA_inprogress = false;

void OTA_WIFI_Task(void *pvParameters) {
  wifiInit();
  WIFI_TB_Init();
  for (;;) {
    WifiOTAHandler();
    vTaskDelay(pdMS_TO_TICKS(OTA_TASK_PERIOD_MS));
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  initEEPROM();
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);

  ESP_LOGI(TAG, "Creating OTA task ...");
  xTaskCreatePinnedToCore(OTA_WIFI_Task, "OTA", 8192, NULL, OTA_TASK_PRIORITY,
                          NULL, CORE_ID_FREERTOS);
  ESP_LOGI(TAG, "OTA task successfully created!");

  ESP_LOGI(TAG, "Creating UI task ...");
  CreateUITask();
  ESP_LOGI(TAG, "UI task successfully created!");
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
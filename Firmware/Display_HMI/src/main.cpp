#include "main.h"
#include "CommTask.h"
#include "UITask.h"
#include "Wifi_OTA.h"
#include "esp_log.h"
#include <PCA9557.h>
#include <lvgl.h>

static const char *TAG = "Main";

bool OTA_inprogress = false;

void setup() {
  Serial.begin(SERIAL_BAUD);
  initEEPROM();
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);

  ESP_LOGI(TAG, "Creating OTA task ...");
  CreateOTATask();
  ESP_LOGI(TAG, "OTA task successfully created!");

  ESP_LOGI(TAG, "Creating Communication task ...");
  CreateCommTask();
  ESP_LOGI(TAG, "Communication task successfully created!");

  ESP_LOGI(TAG, "Creating UI task ...");
  CreateUITask();
  ESP_LOGI(TAG, "UI task successfully created!");
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
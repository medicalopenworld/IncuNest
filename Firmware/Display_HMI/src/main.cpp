#include "main.h"
#include "AudioManager.h"
#include "CommTask.h"
#include "UITask.h"
#include "Wifi_OTA.h"
#include "esp_log.h"
#include <PCA9557.h>
#include <lvgl.h>

static const char *TAG = "Main";

bool OTA_inprogress = false;
in3ator_parameters in3;

void setup() {
  Serial.begin(SERIAL_BAUD);

  // Suppress ESP-IDF gpio error logs (caused by GT911 using pin -1)
  esp_log_level_set("gpio", ESP_LOG_NONE);

  initEEPROM();

  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);

  // AudioManager::getInstance().begin();

  // Backlight enable — pantalla antigua usa PWM directo
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  // Power stability delay
  delay(1000);

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
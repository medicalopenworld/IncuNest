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
#include <Arduino.h>

#include "main.h"

// Logging channels, gated per-channel by their LOG_* flag and serialized
// through the shared log_mutex (main.cpp). Migrated out of system/updateData.cpp,
// which used to mix this with on-board display drawing code.

void logI(String dataString) {
  if (LOG_INFORMATION) {
    static const char *TAG_USER __attribute__((unused)) = "APP";
    if (log_mutex == NULL ||
        xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      // Formato: "123: mensaje"
      ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
      if (log_mutex)
        xSemaphoreGiveRecursive(log_mutex);
    }
  }
}

void logCharger(String dataString) {
  if (LOG_CHARGER) {
    static const char *TAG_USER __attribute__((unused)) = "APP";
    if (log_mutex == NULL ||
        xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      // Formato: "123: mensaje"
      ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
      if (log_mutex)
        xSemaphoreGiveRecursive(log_mutex);
    }
  }
}

void logModemData(String dataString) {
  if (!LOG_MODEM_DATA)
    return;

  static const char *TAG_USER __attribute__((unused)) = "MODEM";
  if (log_mutex == NULL ||
      xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
    if (log_mutex)
      xSemaphoreGiveRecursive(log_mutex);
  }
}

void logE(String dataString) {
  if (!LOG_ERRORS)
    return;

  static const char *TAG_USER __attribute__((unused)) = "APP";
  if (log_mutex == NULL ||
      xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
    if (log_mutex)
      xSemaphoreGiveRecursive(log_mutex);
  }
}

void logAlarm(String dataString) {
  if (!LOG_ALARMS)
    return;

  static const char *TAG_USER __attribute__((unused)) = "APP";
  if (log_mutex == NULL ||
      xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
    if (log_mutex)
      xSemaphoreGiveRecursive(log_mutex);
  }
}

void logSPO2(String dataString) {
  if (!LOG_PULSIOXIMETRY)
    return;

  static const char *TAG_USER __attribute__((unused)) = "SPO2";
  if (log_mutex == NULL ||
      xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
    if (log_mutex)
      xSemaphoreGiveRecursive(log_mutex);
  }
}

void logDrive(String dataString) {
  if (!LOG_DRIVE)
    return;

  static const char *TAG_USER __attribute__((unused)) = "DRIVE";
  if (log_mutex == NULL ||
      xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
    if (log_mutex)
      xSemaphoreGiveRecursive(log_mutex);
  }
}

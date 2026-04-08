#include "CommTask.h"
#include "main.h"
#include <EEPROM.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>

#if (HW_NUM != 16)
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "usb/vcp.hpp"
#include "usb/vcp_ch34x.hpp"
using namespace esp_usb;
#endif

static const char *TAG __attribute__((unused)) = "COMM_HOST";
extern SemaphoreHandle_t log_mutex;
extern char pendingSSID[64];
extern char pendingPass[64];
extern in3ator_parameters in3;

// ======================================================
//  GLOBAL DATA
// ======================================================
TelemetryMessage ctrl_tel_msg = {0, 0, 0, 0};
HMI_CommandMessage hmi_cmd_msg = {0, 0, 0, 0, 0, 0, 0, 0, false};
static HMI_CommandMessage g_last_cmd = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0, false};

static char rxBuffer[256];
static int rxIndex = 0;
static SemaphoreHandle_t hmi_state_req_sem;

#if (HW_NUM == 16)
static HardwareSerial &hmiSerial = Serial1;
#else
static std::unique_ptr<CdcAcmDevice> vcp;
static SemaphoreHandle_t device_disconnected_sem;
static SemaphoreHandle_t vcp_mux;
#endif

// ---- Phototherapy Timer (Motherboard is source of truth) ----
static bool photoTimerActive = false;
static unsigned long photoTimerStartMs = 0;
static int photoTimerMinutes = 0;

// ======================================================
//  USB-ONLY HELPERS
// ======================================================
#if (HW_NUM != 16)
static void reset_vcp() {
  if (xSemaphoreTake(vcp_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (vcp) {
      try {
        vcp->close();
      } catch (...) {
      }
      vcp.reset();
    }
    xSemaphoreGive(vcp_mux);
  } else {
    if (vcp) {
      vcp.reset();
    }
  }
}

static bool handle_rx(const uint8_t *data, size_t len, void *arg) {
  static uint32_t lastRxTime = 0;

  if (rxIndex > 0 && (millis() - lastRxTime > 50)) {
    rxIndex = 0;
  }

  for (size_t i = 0; i < len; i++) {
    char c = data[i];
    if (c == '\r')
      continue;
    if (c == '\n') {
      rxBuffer[rxIndex] = 0;
      parse_line(rxBuffer);
      rxIndex = 0;
      continue;
    }
    if (rxIndex < sizeof(rxBuffer) - 1)
      rxBuffer[rxIndex++] = c;
  }
  lastRxTime = millis();
  return true;
}

static void handle_event(const cdc_acm_host_dev_event_data_t *event,
                         void *user_ctx) {
  if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED) {
    if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGW(TAG, "HMI disconnected event");
      xSemaphoreGiveRecursive(log_mutex);
    }
    xSemaphoreGive(device_disconnected_sem);
  }
}

static void usb_lib_task(void *arg) {
  while (1) {
    uint32_t flags;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
  }
}
#endif // HW_NUM != 16

// ======================================================
//  PHOTOTHERAPY TIMER
// ======================================================
double getRemainingPhotoTime() {
  double remainingTime = 0.0;
  if (photoTimerActive) {
    unsigned long elapsed = millis() - photoTimerStartMs;
    long totalSeconds = (long)photoTimerMinutes * 60;
    long remaining = totalSeconds - (long)(elapsed / 1000);

    if (remaining <= 0) {
      photoTimerActive = false;
      g_last_cmd.phototherapyMode = 0;
      g_last_cmd.photoMinutesRemaining = 0;
      remainingTime = 0.0;

      in3.phototherapy = false;
      ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, 0);
      turnFans(bool(in3.phototherapy || in3.actuation));

      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "Phototherapy timer expired. Hardware turned OFF.");
        xSemaphoreGiveRecursive(log_mutex);
      }
    } else {
      int mins = remaining / 60;
      int secs = remaining % 60;
      remainingTime = mins + (secs / 100.0);
    }
  }
  return remainingTime;
}

// ======================================================
//  SEND STATE TO HMI
// ======================================================
static void send_state_to_hmi() {
  char msg[128];
  int alarmCount = getActiveAlarmCount();
  double remainingTime = getRemainingPhotoTime();

  uint32_t alarmBitmask = 0;
  extern bool alarmOnGoing[];
  for (int i = 0; i < NUM_ALARMS; i++) {
    if (alarmOnGoing[i])
      alarmBitmask |= (1 << i);
  }

  snprintf(msg, sizeof(msg),
           "CTRL,STATE,%d,%d,%.2f,%.2f,%.0f,%d,%d,%d,%d,%c,%s,%d,%d,%d,%.2f,%d,0x%X\n",
           (int)g_last_cmd.actuation, (int)g_last_cmd.controlMode,
           (double)g_last_cmd.desiredAirTemperature,
           (double)g_last_cmd.desiredSkinTemperature,
           (double)g_last_cmd.desiredHumidity, (int)g_last_cmd.phototherapyMode,
           (int)g_last_cmd.muteAlarm, ctrl_tel_msg.serialNumber, HW_NUM,
           HW_REVISION, FWversion, alarmCount, (int)g_last_cmd.skinModeEnabled,
           (int)ctrl_tel_msg.serverCommStatus, remainingTime, in3.language,
           alarmBitmask);

  ESP_LOGI(TAG, "Sending state to HMI: %s", msg);
  CommunicationHost_Send(msg);

  if (alarmCount > 0) {
    resendActiveAlarms();
  }
}

// ======================================================
//  LINE PARSER (common to UART and USB)
// ======================================================
void parse_line(const char *line) {
  if (strncmp(line, EXPECTED_PREFIX, strlen(EXPECTED_PREFIX)) != 0) {
    return;
  }

  if (strcmp(line, "HMI,UI_READY") == 0 || strcmp(line, "HMI,REQ,STATE") == 0) {
    if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGI(TAG, "HMI %s (Queued)", strcmp(line, "HMI,UI_READY") == 0 ? "UI_READY" : "REQ,STATE");
      xSemaphoreGiveRecursive(log_mutex);
    }
    setHMIConnected(true);
    xSemaphoreGive(hmi_state_req_sem);
    return;
  }

  if (strncmp(line, "CTRL,TEL", 8) == 0) {
    double air, skin;
    int hum;
    if (sscanf(line, "CTRL,TEL,%lf,%lf,%d", &air, &skin, &hum) == 3) {
      ctrl_tel_msg.detectedAirTemperature = air;
      ctrl_tel_msg.detectedSkinTemperature = skin;
      ctrl_tel_msg.detectedHumidity = hum;
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "TEL OK air=%.1f skin=%.1f hum=%d", air, skin, hum);
        xSemaphoreGiveRecursive(log_mutex);
      }
    } else {
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGE(TAG, "TEL parse error");
        xSemaphoreGiveRecursive(log_mutex);
      }
    }
    return;
  }

  if (strncmp(line, "CTRL,ALM", 8) == 0) {
    if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGW(TAG, "ALARM: %s", line);
      xSemaphoreGiveRecursive(log_mutex);
    }
    return;
  }

  const char *wifiPtr = strstr(line, "HMI,WIFI");
  if (wifiPtr != NULL) {
    if (sscanf(wifiPtr, "HMI,WIFI,%63[^,],%63s", pendingSSID, pendingPass) == 2) {
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "Received WiFi credentials: SSID=%s, PASS=%s.", pendingSSID, pendingPass);
        xSemaphoreGiveRecursive(log_mutex);
      }
      extern void wifiInit(void);
      wifiInit();
    } else {
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGE(TAG, "WIFI parse error");
        xSemaphoreGiveRecursive(log_mutex);
      }
    }
    return;
  }

  if (strncmp(line, "/config", 7) == 0) {
    char param[32];
    float value;
    bool success = false;
    if (line[7] == ',' && sscanf(line, "/config,%31[^,],%f", param, &value) == 2) {
      success = true;
      extern float maxDesiredTemp[2];
      if (strcmp(param, "FAN_PWM") == 0) {
        in3.fanPWM = (int)value;
        EEPROM.writeInt(EEPROM_FAN_PWM, in3.fanPWM);
      } else if (strcmp(param, "HEATER_AMPS") == 0) {
        in3.heaterMaxPowerAmps = value;
        EEPROM.writeFloat(EEPROM_HEATER_MAX_AMPS, in3.heaterMaxPowerAmps);
      } else if (strcmp(param, "SKIN_TMAX") == 0) {
        in3.skinTemperatureSetMax = value;
        maxDesiredTemp[CONTROL_SKIN] = value;
        EEPROM.writeFloat(EEPROM_SKIN_TEMP_MAX, in3.skinTemperatureSetMax);
      } else if (strcmp(param, "AIR_TMAX") == 0) {
        in3.airTemperatureSetMax = value;
        maxDesiredTemp[CONTROL_AIR] = value;
        EEPROM.writeFloat(EEPROM_AIR_TEMP_MAX, in3.airTemperatureSetMax);
      } else if (strcmp(param, "GPRS_ACT") == 0) {
        in3.actuating_gprs_period = (int)value;
        EEPROM.writeInt(EEPROM_GPRS_ACT_PERIOD, in3.actuating_gprs_period);
      } else if (strcmp(param, "GPRS_PHOTO") == 0) {
        in3.phototherapy_gprs_period = (int)value;
        EEPROM.writeInt(EEPROM_GPRS_PHOTO_PERIOD, in3.phototherapy_gprs_period);
      } else if (strcmp(param, "GPRS_STBY") == 0) {
        in3.standby_gprs_period = (int)value;
        EEPROM.writeInt(EEPROM_GPRS_STBY_PERIOD, in3.standby_gprs_period);
      } else {
        success = false;
      }
      if (success)
        EEPROM.commit();
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (success)
          ESP_LOGI(TAG, "Config updated: %s = %.2f", param, value);
        else
          ESP_LOGE(TAG, "Unknown config parameter: %s", param);
        xSemaphoreGiveRecursive(log_mutex);
      }
    } else {
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "FAN_PWM:%d HEATER_AMPS:%.2f SKIN_TMAX:%.2f AIR_TMAX:%.2f",
                 in3.fanPWM, in3.heaterMaxPowerAmps, in3.skinTemperatureSetMax,
                 in3.airTemperatureSetMax);
        xSemaphoreGiveRecursive(log_mutex);
      }
    }
    return;
  }

  if (strncmp(line, "HMI,", 4) == 0) {
    int act, skinE, mode, photo, mute, lang, photoMin;
    double air, skin, hum;

    if (sscanf(line, "HMI,%d,%d,%d,%lf,%lf,%lf,%d,%d,%d,%d", &act, &skinE,
               &mode, &air, &skin, &hum, &photo, &mute, &lang,
               &photoMin) >= 9) {
      hmi_cmd_msg.actuation = act;
      hmi_cmd_msg.skinModeEnabled = skinE;
      hmi_cmd_msg.controlMode = mode;
      hmi_cmd_msg.desiredAirTemperature = air;
      hmi_cmd_msg.desiredSkinTemperature = skin;
      hmi_cmd_msg.desiredHumidity = hum;
      hmi_cmd_msg.phototherapyMode = photo;
      hmi_cmd_msg.muteAlarm = mute;
      hmi_cmd_msg.language = lang;
      hmi_cmd_msg.photoMinutesRemaining = photoMin;
      hmi_cmd_msg.newCommand = true;

      if (in3.language != lang) {
        in3.language = lang;
        extern void resendActiveAlarms();
        resendActiveAlarms();
      }

      g_last_cmd = hmi_cmd_msg;
      g_last_cmd.newCommand = false;

      if (hmi_cmd_msg.phototherapyMode && hmi_cmd_msg.photoMinutesRemaining > 0) {
        if (!photoTimerActive || photoTimerMinutes != hmi_cmd_msg.photoMinutesRemaining) {
          photoTimerActive = true;
          photoTimerMinutes = hmi_cmd_msg.photoMinutesRemaining;
          photoTimerStartMs = millis();
          if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Phototherapy timer started: %d minutes", photoTimerMinutes);
            xSemaphoreGiveRecursive(log_mutex);
          }
        }
      } else if (!hmi_cmd_msg.phototherapyMode && photoTimerActive) {
        photoTimerActive = false;
        photoTimerMinutes = 0;
        if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          ESP_LOGI(TAG, "Phototherapy timer stopped");
          xSemaphoreGiveRecursive(log_mutex);
        }
      }

      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "HMI CMD stored (lang=%d)", lang);
        xSemaphoreGiveRecursive(log_mutex);
      }
      setHMIConnected(true);
    } else {
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGE(TAG, "HMI parse error");
        xSemaphoreGiveRecursive(log_mutex);
      }
    }
    return;
  }

  if (strlen(line) == 0)
    return;

  if (line[0] == '[' || strncmp(line, "I (", 3) == 0 ||
      strncmp(line, "E (", 3) == 0 || strncmp(line, "W (", 3) == 0 ||
      strncmp(line, "D (", 3) == 0 || strncmp(line, "PC      :", 9) == 0 ||
      strncmp(line, "Backtrace:", 10) == 0 ||
      strncmp(line, "Guru Meditation", 15) == 0 ||
      strncmp(line, "rst:", 4) == 0 || strncmp(line, "boot:", 5) == 0 ||
      strstr(line, "SHA256:") != NULL) {
    return;
  }

  if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGD(TAG, "Unknown line: %s", line);
    xSemaphoreGiveRecursive(log_mutex);
  }
}

// ======================================================
//  SEND DATA TO HMI
// ======================================================
void CommunicationHost_Send(const char *msg) {
#if (HW_NUM == 16)
  hmiSerial.print(msg);
#else
  if (xSemaphoreTake(vcp_mux, pdMS_TO_TICKS(100)) != pdTRUE)
    return;

  if (!vcp) {
    xSemaphoreGive(vcp_mux);
    return;
  }

  size_t len = strlen(msg);
  static uint8_t buf[256];

  if (len >= sizeof(buf)) {
    if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGE(TAG, "TX too long");
      xSemaphoreGiveRecursive(log_mutex);
    }
    xSemaphoreGive(vcp_mux);
    return;
  }

  memcpy(buf, msg, len);
  esp_err_t err = vcp->tx_blocking(buf, len);
  xSemaphoreGive(vcp_mux);

  if (err == ESP_OK) {
    vTaskDelay(pdMS_TO_TICKS(10));
  } else {
    if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGE(TAG, "TX failed: %s", esp_err_to_name(err));
      xSemaphoreGiveRecursive(log_mutex);
    }
    xSemaphoreGive(device_disconnected_sem);
  }
#endif
}

// ======================================================
//  INITIALIZATION
// ======================================================
void CommunicationHost_Init() {
  hmi_state_req_sem = xSemaphoreCreateBinary();

#if (HW_NUM == 16)
  hmiSerial.begin(115200, SERIAL_8N1, UART_MB_RX_PIN, UART_MB_TX_PIN);
  ESP_LOGI(TAG, "UART comm initialized on RX=%d TX=%d",
           UART_MB_RX_PIN, UART_MB_TX_PIN);
#else
  device_disconnected_sem = xSemaphoreCreateBinary();
  vcp_mux = xSemaphoreCreateMutex();

  const usb_host_config_t cfg = {
      .skip_phy_setup = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };
  ESP_ERROR_CHECK(usb_host_install(&cfg));

  xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 10, NULL);
  ESP_ERROR_CHECK(cdc_acm_host_install(NULL));
  VCP::register_driver<CH34x>();
#endif
}

// ======================================================
//  COMMUNICATION TASK
// ======================================================
void Communication_Task(void *pvParameters) {
#if (HW_NUM == 16)
  // ---- UART path ----
  uint32_t last_tel_time = 0;
  ESP_LOGI(TAG, "UART communication task started");

  for (;;) {
    // --- RX: drain Serial1 into line buffer ---
    while (hmiSerial.available()) {
      char c = (char)hmiSerial.read();
      if (c == '\r')
        continue;
      if (c == '\n') {
        rxBuffer[rxIndex] = '\0';
        parse_line(rxBuffer);
        rxIndex = 0;
      } else if (rxIndex < (int)sizeof(rxBuffer) - 1) {
        rxBuffer[rxIndex++] = c;
      }
    }

    // --- STATE request ---
    if (xSemaphoreTake(hmi_state_req_sem, 0) == pdTRUE) {
      send_state_to_hmi();
    }

    // --- Periodic telemetry (every 1 s) ---
    if (millis() - last_tel_time > 1000) {
      int status = COMM_STATUS_NONE;
      if (WIFIIsConnected()) {
        status = WIFIIsConnectedToServer() ? COMM_STATUS_WIFI_SERVER
                                           : COMM_STATUS_WIFI_ONLY;
      } else if (GPRS.connectionStatus) {
        status = GPRSIsConnectedToServer() ? COMM_STATUS_GPRS_SERVER
                                           : COMM_STATUS_GPRS_ONLY;
      }
      ctrl_tel_msg.serverCommStatus = status;

      char msg[64];
      snprintf(msg, sizeof(msg), "CTRL,TEL,%.1f,%.1f,%d,%d\n",
               ctrl_tel_msg.detectedAirTemperature,
               ctrl_tel_msg.detectedSkinTemperature,
               (int)ctrl_tel_msg.detectedHumidity,
               ctrl_tel_msg.serverCommStatus);
      hmiSerial.print(msg);
      last_tel_time = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(COMMUNICATION_TASK_PERIOD_MS));
  }

#else
  // ---- USB CDC/ACM path ----
  while (true) {
    const cdc_acm_host_device_config_t dev = {
        .connection_timeout_ms = 4000,
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .event_cb = handle_event,
        .data_cb = handle_rx,
        .user_arg = NULL,
    };

    if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGI(TAG, "Waiting for HMI...");
      xSemaphoreGiveRecursive(log_mutex);
    }

    std::unique_ptr<CdcAcmDevice> new_vcp;
    try {
      new_vcp = std::unique_ptr<CdcAcmDevice>(VCP::open(&dev));
    } catch (const std::exception &e) {
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGE(TAG, "VCP::open threw exception: %s", e.what());
        xSemaphoreGiveRecursive(log_mutex);
      }
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    if (!new_vcp) {
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGW(TAG, "HMI not found");
        xSemaphoreGiveRecursive(log_mutex);
      }
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    if (xSemaphoreTake(vcp_mux, pdMS_TO_TICKS(500)) == pdTRUE) {
      vcp = std::move(new_vcp);
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "HMI connected!");
        xSemaphoreGiveRecursive(log_mutex);
      }

      bool handshake_success = false;
      for (int retry = 0; retry < 3; retry++) {
        if (vcp) {
          vcp->set_control_line_state(false, false);
          vTaskDelay(pdMS_TO_TICKS(500));
          vcp->set_control_line_state(true, true);
          handshake_success = true;
        }
        if (handshake_success)
          break;
        vTaskDelay(pdMS_TO_TICKS(200));
      }

      vTaskDelay(pdMS_TO_TICKS(50));

      cdc_acm_line_coding_t line = {
          .dwDTERate = 115200, .bCharFormat = 0, .bParityType = 0, .bDataBits = 8};

      bool init_success = false;
      esp_err_t err_coding = ESP_FAIL;
      for (int i = 0; i < 3; i++) {
        err_coding = vcp->line_coding_set(&line);
        if (err_coding == ESP_OK) {
          init_success = true;
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
      }

      if (!init_success) {
        if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          ESP_LOGE(TAG, "line_coding_set failed: %s", esp_err_to_name(err_coding));
          xSemaphoreGiveRecursive(log_mutex);
        }
        xSemaphoreGive(vcp_mux);
        reset_vcp();
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }
      xSemaphoreGive(vcp_mux);
    } else {
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGE(TAG, "Failed to take vcp_mux");
        xSemaphoreGiveRecursive(log_mutex);
      }
      continue;
    }

    xSemaphoreTake(device_disconnected_sem, 0);
    uint32_t last_tel_time = 0;

    while (true) {
      if (xSemaphoreTake(hmi_state_req_sem, 0) == pdTRUE) {
        send_state_to_hmi();
      }

      if (millis() - last_tel_time > 1000) {
        int status = COMM_STATUS_NONE;
        if (WIFIIsConnected()) {
          status = WIFIIsConnectedToServer() ? COMM_STATUS_WIFI_SERVER
                                             : COMM_STATUS_WIFI_ONLY;
        } else if (GPRS.connectionStatus) {
          status = GPRSIsConnectedToServer() ? COMM_STATUS_GPRS_SERVER
                                             : COMM_STATUS_GPRS_ONLY;
        }
        ctrl_tel_msg.serverCommStatus = status;

        char msg[64];
        snprintf(msg, sizeof(msg), "CTRL,TEL,%.1f,%.1f,%d,%d\n",
                 ctrl_tel_msg.detectedAirTemperature,
                 ctrl_tel_msg.detectedSkinTemperature,
                 (int)ctrl_tel_msg.detectedHumidity,
                 ctrl_tel_msg.serverCommStatus);

        if (xSemaphoreTake(vcp_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
          if (!vcp) {
            xSemaphoreGive(vcp_mux);
            break;
          }
          esp_err_t err = vcp->tx_blocking((uint8_t *)msg, strlen(msg));
          xSemaphoreGive(vcp_mux);
          if (err != ESP_OK) {
            if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
              ESP_LOGE(TAG, "Periodic TX failed: %s", esp_err_to_name(err));
              xSemaphoreGiveRecursive(log_mutex);
            }
          }
        }
        last_tel_time = millis();
      }

      if (xSemaphoreTake(device_disconnected_sem, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          ESP_LOGW(TAG, "Disconnect requested");
          xSemaphoreGiveRecursive(log_mutex);
        }
        break;
      }
    }

    if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGW(TAG, "Closing VCP and retrying...");
      xSemaphoreGiveRecursive(log_mutex);
    }
    reset_vcp();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
#endif // HW_NUM == 16
}

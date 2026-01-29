#include "CommTask.h"
#include "main.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>

#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "usb/vcp.hpp"
#include "usb/vcp_ch34x.hpp"

using namespace esp_usb;

static const char *TAG = "COMM_HOST";
extern SemaphoreHandle_t log_mutex;
extern char pendingSSID[64];
extern char pendingPass[64];
extern in3ator_parameters in3;
// ======================================================
//  GLOBAL DATA
// ======================================================
TelemetryMessage ctrl_tel_msg = {0, 0, 0, 0};
HMI_CommandMessage hmi_cmd_msg = {0, 0, 0, 0, 0, 0, 0, 0, false};
// ---- NUEVO: cache de estado “último comando/setpoints” ----
static HMI_CommandMessage g_last_cmd = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0, false};

static std::unique_ptr<CdcAcmDevice> vcp;
static char rxBuffer[256];
static int rxIndex = 0;

static SemaphoreHandle_t device_disconnected_sem;
static SemaphoreHandle_t vcp_mux;
static SemaphoreHandle_t hmi_state_req_sem;

// ---- Phototherapy Timer (Motherboard is source of truth) ----
static bool photoTimerActive = false;
static unsigned long photoTimerStartMs = 0;
static int photoTimerMinutes = 0;

static void reset_vcp() {
  if (xSemaphoreTake(vcp_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (vcp) {
      // Try to close, but don't fail if device already in bad state
      try {
        vcp->close();
      } catch (...) {
        // Ignore close errors - device may already be disconnected
      }
      vcp.reset();
    }
    xSemaphoreGive(vcp_mux);
  } else {
    // If we can't take the mux, we still need to try to clean up
    if (vcp) {
      vcp.reset();
    }
  }
}

// ---- NUEVO: enviar CTRL,STATE ---- CTRL,STATE,1,0,36.50,36.80,55,1,0,1,123456,2,1,v1.3.0
static void send_state_to_hmi() {
  char msg[128];
  int alarmCount = getActiveAlarmCount();
  
  // Calculate real remaining time if phototherapy is active (formato MM.SS)
  double remainingTime = 0.0;
  if (photoTimerActive) {
      unsigned long elapsed = millis() - photoTimerStartMs;
      long totalSeconds = photoTimerMinutes * 60;
      long remaining = totalSeconds - (elapsed / 1000);
      
      if (remaining <= 0) {
          // Timer expired, turn off phototherapy
          photoTimerActive = false;
          g_last_cmd.phototherapyMode = 0;
          g_last_cmd.photoMinutesRemaining = 0;
          remainingTime = 0.0;
      } else {
          // Calculate MM.SS format: 18 min 33 sec = 18.33
          int mins = remaining / 60;
          int secs = remaining % 60;
          remainingTime = mins + (secs / 100.0);
      }
  }
  
  snprintf(msg, sizeof(msg),
           "CTRL,STATE,%d,%d,%.2f,%.2f,%.0f,%d,%d,%d,%d,%c,%s,%d,%d,%d,%.2f\n",
           (int)g_last_cmd.actuation, (int)g_last_cmd.controlMode,
           (double)g_last_cmd.desiredAirTemperature,
           (double)g_last_cmd.desiredSkinTemperature,
           (double)g_last_cmd.desiredHumidity, (int)g_last_cmd.phototherapyMode,
           (int)g_last_cmd.muteAlarm,
           ctrl_tel_msg.serialNumber, HW_NUM, HW_REVISION, FWversion, alarmCount, (int)g_last_cmd.skinModeEnabled, (int)ctrl_tel_msg.serverCommStatus,
           remainingTime);
  ESP_LOGI(TAG, "Sending state to HMI: %s", msg);
  CommunicationHost_Send(msg);
  
  // Trigger alarm resend if any
  if (alarmCount > 0) {
      resendActiveAlarms();
  }
}

void parse_line(const char *line) {
  // ESP_LOGD(TAG, "RX: %s", line); // Removed to avoid UART flooding

  // Strict filtering: discard if not starting with EXPECTED_PREFIX
  if (strncmp(line, EXPECTED_PREFIX, strlen(EXPECTED_PREFIX)) != 0) {
    return;
  }

  // ---- NUEVO: handshake request ----
  if (strcmp(line, "HMI,REQ,STATE") == 0) {
    if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGI(TAG, "HMI requested STATE (Queued)");
      xSemaphoreGiveRecursive(log_mutex);
    }
    setHMIConnected(true);             // Flush pending alarms
    xSemaphoreGive(hmi_state_req_sem); // Signal task to send response
    return;
  }

  // -----------------------------
  // CTRL,TEL
  // -----------------------------
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

  // -----------------------------
  // CTRL,ALM
  // -----------------------------
  if (strncmp(line, "CTRL,ALM", 8) == 0) {
    if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGW(TAG, "ALARM: %s", line);
      xSemaphoreGiveRecursive(log_mutex);
    }
    return;
  }

  // -----------------------------
  // HMI WIFI
  // -----------------------------
  const char *wifiPtr = strstr(line, "HMI,WIFI");
  if (wifiPtr != NULL) {
    if (sscanf(wifiPtr, "HMI,WIFI,%63[^,],%63s", pendingSSID, pendingPass) ==
        2) {
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG,
                 "Received WiFi credentials: SSID=%s, PASS=%s. Attempting "
                 "connection...",
                 pendingSSID, pendingPass);
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

  // -----------------------------
  // HMI COMMAND
  // -----------------------------
  if (strncmp(line, "HMI,", 4) == 0) {
    int act, skinE, mode, photo, mute, lang, photoMin;
    double air, skin, hum;

    if (sscanf(line, "HMI,%d,%d,%d,%lf,%lf,%lf,%d,%d,%d,%d", &act, &skinE, &mode, &air, &skin,
               &hum, &photo, &mute, &lang, &photoMin) >= 9) {
      
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

      // ---- NUEVO: actualiza cache (para futuras respuestas CTRL,STATE) ----
      g_last_cmd = hmi_cmd_msg;
      g_last_cmd.newCommand = false;
      
      // ---- Phototherapy Timer Management ----
      if (hmi_cmd_msg.phototherapyMode && hmi_cmd_msg.photoMinutesRemaining > 0) {
          // Start or restart phototherapy timer
          if (!photoTimerActive || photoTimerMinutes != hmi_cmd_msg.photoMinutesRemaining) {
              photoTimerActive = true;
              photoTimerMinutes = hmi_cmd_msg.photoMinutesRemaining;
              photoTimerStartMs = millis();
              if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                  ESP_LOGI(TAG, "Phototherapy timer started: %d minutes", photoTimerMinutes);
                  xSemaphoreGiveRecursive(log_mutex);
              }
          }
      } else if (!hmi_cmd_msg.phototherapyMode) {
          // Turn off phototherapy
          if (photoTimerActive) {
              photoTimerActive = false;
              photoTimerMinutes = 0;
              if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                  ESP_LOGI(TAG, "Phototherapy timer stopped");
                  xSemaphoreGiveRecursive(log_mutex);
              }
          }
      }

      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "HMI CMD stored successfully (lang=%d)", lang);
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

  // Ignore empty lines
  if (strlen(line) == 0)
    return;

  // HMI logs/crash dumps: Ignore lines starting with '[' or typical ESP32 crash
  // keywords
  if (line[0] == '[' || strncmp(line, "I (", 3) == 0 ||
      strncmp(line, "E (", 3) == 0 || strncmp(line, "W (", 3) == 0 ||
      strncmp(line, "D (", 3) == 0 || strncmp(line, "PC      :", 9) == 0 ||
      strncmp(line, "PS      :", 9) == 0 ||
      strncmp(line, "A0      :", 9) == 0 ||
      strncmp(line, "A10     :", 9) == 0 ||
      strncmp(line, "EXCCAUSE:", 9) == 0 ||
      strncmp(line, "EXCVADDR:", 9) == 0 ||
      strncmp(line, "Backtrace:", 10) == 0 ||
      strncmp(line, "Core  1", 7) == 0 ||
      strncmp(line, "Guru Meditation", 15) == 0 ||
      strncmp(line, "Rebooting", 9) == 0 || strncmp(line, "Build:", 6) == 0 ||
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
//  USB RX CALLBACK
// ======================================================
static bool handle_rx(const uint8_t *data, size_t len, void *arg) {
  static uint32_t lastRxTime = 0;

  // Timeout check
  if (rxIndex > 0 && (millis() - lastRxTime > 50)) {
    rxIndex = 0;
    // We can log here if needed, but be careful in callback
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

// ======================================================
//  USB EVENT (disconnect)
// ======================================================
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

// ======================================================
//  USB host service task
// ======================================================
static void usb_lib_task(void *arg) {
  while (1) {
    uint32_t flags;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
  }
}

// ======================================================
//  SEND DATA TO HMI
// ======================================================
void CommunicationHost_Send(const char *msg) {
  if (xSemaphoreTake(vcp_mux, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }

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

  if (err != ESP_OK) {
    if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGE(TAG, "TX failed: %s", esp_err_to_name(err));
      xSemaphoreGiveRecursive(log_mutex);
    }
    xSemaphoreGive(device_disconnected_sem); // Trigger reconnection
  }
}

// ======================================================
//  INITIALIZATION (CALL ONCE IN setup())
// ======================================================
void CommunicationHost_Init() {
  device_disconnected_sem = xSemaphoreCreateBinary();
  hmi_state_req_sem = xSemaphoreCreateBinary();
  vcp_mux = xSemaphoreCreateMutex();

  const usb_host_config_t cfg = {
      .skip_phy_setup = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };
  ESP_ERROR_CHECK(usb_host_install(&cfg));

  xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 10, NULL);

  ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

  VCP::register_driver<CH34x>();
  // Drivers CP210x y FTDI eliminados para optimizar para CH340C
  // VCP::register_driver<CP210x>();
  // VCP::register_driver<FT23x>();
}

// ======================================================
//  COMMUNICATION TASK
// ======================================================
void Communication_Task(void *pvParameters) {
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

      // --- HMI BOOT FIX: Ensure stable state before enabling DTR/RTS ---
      // CH340C needs a defined transition. We try a robust handshake sequence.

      bool handshake_success = false;
      for (int retry = 0; retry < 3; retry++) {
        if (vcp) {
          vcp->set_control_line_state(false, false);
          vTaskDelay(pdMS_TO_TICKS(500));          // Wait for electrical settle
          vcp->set_control_line_state(true, true); // Enable DTR/RTS
          handshake_success = true;
        }

        if (handshake_success)
          break;
        vTaskDelay(pdMS_TO_TICKS(200));
      }

      if (!handshake_success) {
        if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          ESP_LOGW(TAG, "Handshake sequence incomplete (mux busy or vcp null)");
          xSemaphoreGiveRecursive(log_mutex);
        }
      }

      vTaskDelay(pdMS_TO_TICKS(50));

      // Line coding - INSIDE MUTEX
      cdc_acm_line_coding_t line = {.dwDTERate = 115200,
                                    .bCharFormat = 0,
                                    .bParityType = 0,
                                    .bDataBits = 8};

      bool init_success = false;
      esp_err_t err_coding = ESP_FAIL;
      for (int i = 0; i < 3; i++) {
        err_coding = vcp->line_coding_set(&line);
        if (err_coding == ESP_OK) {
          init_success = true;
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait before retry
      }

      if (!init_success) {
        if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          ESP_LOGE(TAG, "line_coding_set failed after retries: %s",
                   esp_err_to_name(err_coding));
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
        ESP_LOGE(TAG, "Failed to take vcp_mux for VCP assignment/init");
        xSemaphoreGiveRecursive(log_mutex);
      }
      continue;
    }

    xSemaphoreTake(device_disconnected_sem, 0); // Clear any pending disconnect
    uint32_t last_tel_time = 0;

    // COMM LOOP
    while (true) {
      // 1. Check for STATE Request
      if (xSemaphoreTake(hmi_state_req_sem, 0) == pdTRUE) {
        send_state_to_hmi();
      }

      // 2. Periodic Telemetry (every ~1000ms)
      if (millis() - last_tel_time > 1000) {
        // Determine communication status
        int status = COMM_STATUS_NONE;
        if (WIFIIsConnected()) {
          if (WIFIIsConnectedToServer()) {
            status = COMM_STATUS_WIFI_SERVER;
          } else {
            status = COMM_STATUS_WIFI_ONLY;
          }
        } else if (GPRS.connectionStatus) {
          if (GPRSIsConnectedToServer()) {
            status = COMM_STATUS_GPRS_SERVER;
          } else {
            status = COMM_STATUS_GPRS_ONLY;
          }
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
            break; // Should affect next loop iteration check
          }
          esp_err_t err = vcp->tx_blocking((uint8_t *)msg, strlen(msg));
          xSemaphoreGive(vcp_mux);
          if (err != ESP_OK) {
            if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) ==
                pdTRUE) {
              ESP_LOGE(TAG, "Periodic TX failed: %s", esp_err_to_name(err));
              xSemaphoreGiveRecursive(log_mutex);
            }
            // If TX fails, it might be disconnected.
            // The disconnect event usually comes separately, but we can break
            // or continue. Let's continue and let the event handler or next
            // init handle it.
          }
        }
        last_tel_time = millis();
      }

      // 3. Check for Disconnect Event
      if (xSemaphoreTake(device_disconnected_sem, pdMS_TO_TICKS(50)) ==
          pdTRUE) {
        if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          ESP_LOGW(TAG, "Disconnect requested or event received");
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
}
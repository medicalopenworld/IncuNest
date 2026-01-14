#include "communication_host.h"
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
#include "usb/vcp_cp210x.hpp"
#include "usb/vcp_ftdi.hpp"

using namespace esp_usb;

static const char *TAG = "COMM_HOST";

// ======================================================
//  GLOBAL DATA
// ======================================================
TelemetryMessage ctrl_tel_msg = {0, 0, 0};
HMI_CommandMessage hmi_cmd_msg = {0, 0, 0, 0, 0, 0, 0, false};
// ---- NUEVO: cache de estado “último comando/setpoints” ----
static HMI_CommandMessage g_last_cmd = {0, 0, 0, 0, 0, 0, 0, false};

static std::unique_ptr<CdcAcmDevice> vcp;
static char rxBuffer[256];
static int rxIndex = 0;

static SemaphoreHandle_t device_disconnected_sem;
static SemaphoreHandle_t vcp_mux;

static void reset_vcp() {
  if (xSemaphoreTake(vcp_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (vcp) {
      vcp->close();
      vcp.reset();
    }
    xSemaphoreGive(vcp_mux);
  }
}

// ---- NUEVO: enviar CTRL,STATE ----
static void send_state_to_hmi() {
  // Si aún no hubo comando nunca, puedes rellenar con defaults
  // (aquí usamos cache, que se actualizará cuando llegue HMI real).
  char msg[128];
  snprintf(msg, sizeof(msg), "CTRL,STATE,%d,%d,%.2f,%.2f,%.0f,%d,%d\n",
           (int)g_last_cmd.actuation, (int)g_last_cmd.controlMode,
           (double)g_last_cmd.desiredAirTemperature,
           (double)g_last_cmd.desiredSkinTemperature,
           (double)g_last_cmd.desiredHumidity, (int)g_last_cmd.phototherapyMode,
           (int)g_last_cmd.muteAlarm);

  CommunicationHost_Send(msg);
}

// ======================================================
//  PARSER
// ======================================================
char pendingSSID[64] = "";
char pendingPass[64] = "";

void parse_line(const char *line) {
  ESP_LOGD(TAG, "RX: %s", line);

  // ---- NUEVO: handshake request ----
  if (strcmp(line, "HMI,REQ,STATE") == 0) {
    ESP_LOGI(TAG, "HMI requested STATE");
    send_state_to_hmi();
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

      ESP_LOGI(TAG, "TEL OK air=%.1f skin=%.1f hum=%d", air, skin, hum);
    } else
      ESP_LOGE(TAG, "TEL parse error");
    return;
  }

  // -----------------------------
  // CTRL,ALM
  // -----------------------------
  if (strncmp(line, "CTRL,ALM", 8) == 0) {
    ESP_LOGW(TAG, "ALARM: %s", line);
    return;
  }

  // -----------------------------
  // HMI WIFI
  // -----------------------------
  const char *wifiPtr = strstr(line, "HMI,WIFI");
  if (wifiPtr != NULL) {
    if (sscanf(wifiPtr, "HMI,WIFI,%[^,],%s", pendingSSID, pendingPass) == 2) {
      ESP_LOGI(TAG,
               "Received WiFi credentials: SSID=%s, PASS=***. Attempting "
               "connection...",
               pendingSSID);
      extern void wifiInit(void);
      wifiInit();
    } else
      ESP_LOGE(TAG, "WIFI parse error");
    return;
  }

  // -----------------------------
  // HMI COMMAND
  // -----------------------------
  if (strncmp(line, "HMI,", 4) == 0) {
    int act, mode, photo, mute;
    double air, skin, hum;

    if (sscanf(line, "HMI,%d,%d,%lf,%lf,%lf,%d,%d", &act, &mode, &air, &skin,
               &hum, &photo, &mute) == 7) {
      hmi_cmd_msg.actuation = act;
      hmi_cmd_msg.controlMode = mode;
      hmi_cmd_msg.desiredAirTemperature = air;
      hmi_cmd_msg.desiredSkinTemperature = skin;
      hmi_cmd_msg.desiredHumidity = hum;
      hmi_cmd_msg.phototherapyMode = photo;
      hmi_cmd_msg.muteAlarm = mute;
      hmi_cmd_msg.newCommand = true;

      // ---- NUEVO: actualiza cache (para futuras respuestas CTRL,STATE) ----
      g_last_cmd = hmi_cmd_msg;
      g_last_cmd.newCommand = false;

      ESP_LOGI(TAG, "HMI CMD stored successfully");
    } else
      ESP_LOGE(TAG, "HMI parse error");

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

  ESP_LOGD(TAG, "Unknown line: %s", line);
}

// ======================================================
//  USB RX CALLBACK
// ======================================================
static bool handle_rx(const uint8_t *data, size_t len, void *arg) {
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
  return true;
}

// ======================================================
//  USB EVENT (disconnect)
// ======================================================
static void handle_event(const cdc_acm_host_dev_event_data_t *event,
                         void *user_ctx) {
  if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED) {
    ESP_LOGW(TAG, "HMI disconnected event");
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
    ESP_LOGE(TAG, "TX too long");
    xSemaphoreGive(vcp_mux);
    return;
  }

  memcpy(buf, msg, len);
  esp_err_t err = vcp->tx_blocking(buf, len);
  xSemaphoreGive(vcp_mux);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "TX failed: %s", esp_err_to_name(err));
    xSemaphoreGive(device_disconnected_sem); // Trigger reconnection
  }
}

// ======================================================
//  INITIALIZATION (CALL ONCE IN setup())
// ======================================================
void CommunicationHost_Init() {
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
  VCP::register_driver<CP210x>();
  VCP::register_driver<FT23x>();
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

    ESP_LOGI(TAG, "Waiting for HMI...");
    std::unique_ptr<CdcAcmDevice> new_vcp;
    try {
      new_vcp = std::unique_ptr<CdcAcmDevice>(VCP::open(&dev));
    } catch (const std::exception &e) {
      ESP_LOGE(TAG, "VCP::open threw exception: %s", e.what());
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    if (!new_vcp) {
      ESP_LOGW(TAG, "HMI not found");
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    if (xSemaphoreTake(vcp_mux, pdMS_TO_TICKS(500)) == pdTRUE) {
      vcp = std::move(new_vcp);
      ESP_LOGI(TAG, "HMI connected!");

      // Enable DTR/RTS (CH340C requirement) - INSIDE MUTEX
      vcp->set_control_line_state(true, true);
      vTaskDelay(pdMS_TO_TICKS(20));

      // Line coding - INSIDE MUTEX
      cdc_acm_line_coding_t line = {.dwDTERate = 115200,
                                    .bCharFormat = 0,
                                    .bParityType = 0,
                                    .bDataBits = 8};
      vcp->line_coding_set(&line);
      xSemaphoreGive(vcp_mux);
    } else {
      ESP_LOGE(TAG, "Failed to take vcp_mux for VCP assignment/init");
      continue;
    }

    xSemaphoreTake(device_disconnected_sem, 0); // Clear any pending disconnect

    // COMM LOOP
    while (true) {
      char msg[64];
      snprintf(msg, sizeof(msg), "CTRL,TEL,%.1f,%.1f,%d\n",
               ctrl_tel_msg.detectedAirTemperature,
               ctrl_tel_msg.detectedSkinTemperature,
               (int)ctrl_tel_msg.detectedHumidity);

      if (xSemaphoreTake(vcp_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!vcp) {
          xSemaphoreGive(vcp_mux);
          break;
        }
        esp_err_t err = vcp->tx_blocking((uint8_t *)msg, strlen(msg));
        xSemaphoreGive(vcp_mux);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "Periodic TX failed: %s", esp_err_to_name(err));
          break;
        }
      }

      if (xSemaphoreTake(device_disconnected_sem, pdMS_TO_TICKS(1000)) ==
          pdTRUE) {
        ESP_LOGW(TAG, "Disconnect requested or event received");
        break;
      }
    }

    ESP_LOGW(TAG, "Closing VCP and retrying...");
    reset_vcp();
  }
}
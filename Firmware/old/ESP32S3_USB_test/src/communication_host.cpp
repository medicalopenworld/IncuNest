#include "CommTask.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "string.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "usb/vcp.hpp"
#include "usb/vcp_ch34x.hpp"
#include "usb/vcp_cp210x.hpp"
#include "usb/vcp_ftdi.hpp"

using namespace esp_usb;

static const char *TAG = "COMM_HOST";

// ===========================
// GLOBAL EXPORTED VARIABLES
// ===========================
TelemetryMessage host_tel_msg = {0};
AlarmMessage host_alarm_msg = {0};
HMIMessage host_hmi_msg = {0};

// ===========================
static std::unique_ptr<CdcAcmDevice> vcp;

// ===========================
// RX BUFFER FOR LINE ASSEMBLY
// ===========================
static char rxBuffer[256];
static int rxIndex = 0;

// Forward declarations
static void parse_line(const char *line);
static void usb_lib_task(void *arg);
static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *ctx);

// Semaphore for disconnect event
static SemaphoreHandle_t device_disconnected_sem;

// ==================================
// SAFE TX: buffer copy
// ==================================
void CommunicationHost_Send(const char *msg) {
  if (!vcp)
    return;

  size_t len = strlen(msg);
  static uint8_t buf[256];

  if (len >= sizeof(buf)) {
    ESP_LOGE(TAG, "TX message too long");
    return;
  }

  memcpy(buf, msg, len);
  vcp->tx_blocking(buf, len);
}

// ==================================
// RX CALLBACK (raw bytes)
// ==================================
bool CommunicationHost_ReceiveBytes(const uint8_t *data, size_t len,
                                    void *arg) {
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

// ==================================
// PARSER – YOUR TEXT PROTOCOL
// ==================================
static void parse_line(const char *line) {
  ESP_LOGI(TAG, "RX: %s", line);

  // -------------------------
  // CTRL,TEL
  // -------------------------
  if (strncmp(line, "CTRL,TEL", 8) == 0) {
    double air, skin;
    int hum;

    int r = sscanf(line, "CTRL,TEL,%lf,%lf,%d", &air, &skin, &hum);
    if (r == 3) {
      host_tel_msg.detectedAirTemperature = air;
      host_tel_msg.detectedSkinTemperature = skin;
      host_tel_msg.detectedHumidity = hum;

      ESP_LOGI(TAG, "TEL OK: %.1f, %.1f, %d", air, skin, hum);
    } else {
      ESP_LOGE(TAG, "Parse error TEL");
    }
    return;
  }

  // -------------------------
  // CTRL,ALM
  // -------------------------
  if (strncmp(line, "CTRL,ALM", 8) == 0) {
    int id, stateInt;
    char type[32];
    char desc[128];

    int r =
        sscanf(line, "CTRL,ALM,%d,%[^,],%[^,],%d", &id, type, desc, &stateInt);

    if (r == 4) {
      host_alarm_msg.id = id;
      host_alarm_msg.state = stateInt;
      strncpy(host_alarm_msg.type, type, sizeof(host_alarm_msg.type));
      strncpy(host_alarm_msg.description, desc,
              sizeof(host_alarm_msg.description));

      ESP_LOGI(TAG, "ALARM OK: %d, %s, %d", id, type, stateInt);
    } else {
      ESP_LOGE(TAG, "Parse error ALM");
    }
    return;
  }

  // -------------------------
  // HMI,<...>
  // -------------------------
  if (strncmp(line, "HMI,", 4) == 0) {
    int act, mode, photo, mute;
    double air, skin, hum;

    int r = sscanf(line, "HMI,%d,%d,%lf,%lf,%lf,%d,%d", &act, &mode, &air,
                   &skin, &hum, &photo, &mute);

    if (r == 7) {
      host_hmi_msg.actuation = act;
      host_hmi_msg.controlMode = mode;
      host_hmi_msg.desiredAirTemperature = air;
      host_hmi_msg.desiredSkinTemperature = skin;
      host_hmi_msg.desiredHumidity = hum;
      host_hmi_msg.phototherapyMode = photo;
      host_hmi_msg.muteAlarm = mute;

      ESP_LOGI(TAG, "HMI OK");
    } else {
      ESP_LOGE(TAG, "Parse error HMI");
    }
    return;
  }

  ESP_LOGW(TAG, "Unknown message: %s", line);
}

// ==================================
// USB EVENT HANDLER
// ==================================
static void handle_event(const cdc_acm_host_dev_event_data_t *event,
                         void *ctx) {
  if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED) {
    ESP_LOGW(TAG, "USB device disconnected");
    xSemaphoreGive(device_disconnected_sem);
  }
}

// ==================================
// USB HOST LOW-LEVEL EVENT TASK
// ==================================
static void usb_lib_task(void *arg) {
  while (true) {
    uint32_t event_flags;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
  }
}

// ==================================
// MAIN INIT FUNCTION
// ==================================
void CommunicationHost_Init() {
  device_disconnected_sem = xSemaphoreCreateBinary();

  ESP_LOGI(TAG, "Initializing USB Host...");

  const usb_host_config_t host_cfg = {
      .skip_phy_setup = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };

  ESP_ERROR_CHECK(usb_host_install(&host_cfg));
  xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 10, NULL);

  ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

  // Register USB-to-UART drivers
  VCP::register_driver<CH34x>();
  VCP::register_driver<CP210x>();
  VCP::register_driver<FT23x>();

  // Wait for a HMI device
  while (true) {
    const cdc_acm_host_device_config_t cfg = {
        .connection_timeout_ms = 4000,
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .event_cb = handle_event,
        .data_cb = CommunicationHost_ReceiveBytes,
        .user_arg = NULL,
    };

    ESP_LOGI(TAG, "Waiting for HMI...");
    vcp = std::unique_ptr<CdcAcmDevice>(VCP::open(&cfg));

    if (vcp == nullptr) {
      ESP_LOGW(TAG, "No VCP device detected");
      continue;
    }

    ESP_LOGI(TAG, "HMI USB connected!");
    break;
  }
}

#include "CommTask.h"
#include "main.h"
#include "DriveUpload.h"
#include <EEPROM.h>
#include <LittleFS.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>
#include <time.h>

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
// Bloquea nuevos TX en cuanto se detecta desconexión para que el close() no
// curse con URBs en vuelo (evita assert en hcd_urb_dequeue del USB host).
static volatile bool vcp_disconnecting = false;
#endif

// ---- Phototherapy Timer (Motherboard is source of truth) ----
static bool photoTimerActive = false;
static unsigned long photoTimerStartMs = 0;
static int photoTimerMinutes = 0;

// ======================================================
//  USB-ONLY HELPERS
// ======================================================
void parse_line(const char *line);

#if (HW_NUM != 16)
static void reset_vcp() {
  vcp_disconnecting = true;
  // Espera hasta 2s a que termine un tx_blocking en curso antes de cerrar.
  if (xSemaphoreTake(vcp_mux, pdMS_TO_TICKS(2000)) == pdTRUE) {
    if (vcp) {
      try {
        vcp->close();
      } catch (...) {
      }
      // Deja que cdc_acm_client_task drene URBs pendientes antes de liberar
      // el objeto; si no, el dequeue async asserta en hcd_dwc.c.
      vTaskDelay(pdMS_TO_TICKS(200));
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
    vcp_disconnecting = true;
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
  char msg[160];
  int alarmCount = getActiveAlarmCount();
  double remainingTime = getRemainingPhotoTime();

  uint32_t alarmBitmask = 0;
  extern bool alarmOnGoing[];
  for (int i = 0; i < NUM_ALARMS; i++) {
    if (alarmOnGoing[i])
      alarmBitmask |= (1 << i);
  }

  // Derive probe state from skin temperature: >0.1°C means probe is physically connected
  int skinProbeState = (in3.temperature[SKIN_SENSOR] > 0.1f) ? SKIN_PROBE_VALID
                                                              : SKIN_PROBE_NOT_CONNECTED;

  snprintf(msg, sizeof(msg),
           "CTRL,STATE,%d,%d,%.2f,%.2f,%.0f,%d,%d,%d,%d,%c,%s,%d,%d,%d,%.2f,%d,%d,0x%X\n",
           (int)g_last_cmd.actuation, (int)g_last_cmd.controlMode,
           (double)g_last_cmd.desiredAirTemperature,
           (double)g_last_cmd.desiredSkinTemperature,
           (double)g_last_cmd.desiredHumidity, (int)g_last_cmd.phototherapyMode,
           (int)g_last_cmd.muteAlarm, ctrl_tel_msg.serialNumber, HW_NUM,
           HW_REVISION, FWversion, alarmCount, (int)g_last_cmd.skinModeEnabled,
           (int)ctrl_tel_msg.serverCommStatus, remainingTime, in3.language,
           skinProbeState, alarmBitmask);

  ESP_LOGI(TAG, "Sending state to HMI: %s", msg);
  CommunicationHost_Send(msg);

  if (alarmCount > 0) {
    resendActiveAlarms();
  }
}

// ======================================================
//  HMI PANIC CAPTURE
//
//  The HMI board's UART0 (boot/panic output) is wired to this motherboard's
//  hmiSerial. Regular protocol lines start with "HMI,"/"CTRL," etc., but when
//  the HMI panics the ESP32 ROM dumps register/backtrace text we want to keep.
//  We buffer those lines into LittleFS and hand them off to the Drive uploader.
// ======================================================
#define HMI_CRASH_BUF_SIZE    4096
#define HMI_CRASH_TIMEOUT_MS  10000UL

static char     hmi_crash_buf[HMI_CRASH_BUF_SIZE];
static int      hmi_crash_len        = 0;
static bool     hmi_crash_capturing  = false;
static uint32_t hmi_crash_last_ms    = 0;
static uint32_t hmi_crash_start_ms   = 0;

static bool lineStartsCrash(const char *line) {
  return strncmp(line, "Guru Meditation", 15) == 0 ||
         strncmp(line, "abort() was called", 18) == 0 ||
         strncmp(line, "Stack canary", 12) == 0 ||
         strstr(line, "Task watchdog got triggered") != NULL ||
         strncmp(line, "Debug exception", 15) == 0 ||
         strstr(line, "assert failed") != NULL ||
         strncmp(line, "PC      :", 9) == 0 ||
         strncmp(line, "Backtrace:", 10) == 0 ||
         strstr(line, "LoadProhibited") != NULL ||
         strstr(line, "StoreProhibited") != NULL;
}

static bool lineEndsCrash(const char *line) {
  // Boot ROM banner signals the HMI has rebooted past the panic.
  return strncmp(line, "rst:", 4) == 0 || strncmp(line, "ets ", 4) == 0;
}

static void hmiCrashFlush() {
  if (hmi_crash_len <= 0) {
    hmi_crash_capturing = false;
    hmi_crash_len       = 0;
    return;
  }

  char path[48];
  snprintf(path, sizeof(path), "/crash_hmi_%lu.log",
           (unsigned long)hmi_crash_start_ms);

  File f = LittleFS.open(path, "w", true);
  if (!f) {
    logDrive(String("HMI crash: cannot open ") + path);
    hmi_crash_capturing = false;
    hmi_crash_len       = 0;
    return;
  }
  f.printf("=== IncuNest display_HMI panic capture ===\n");
  f.printf("Captured by motherboard FW %s (SN %d)\n", FWversion,
           (int)in3.serialNumber);
  f.printf("Capture window: %lu ms\n",
           (unsigned long)(hmi_crash_last_ms - hmi_crash_start_ms));
  f.printf("-- begin --\n");
  f.write((const uint8_t *)hmi_crash_buf, hmi_crash_len);
  f.printf("\n-- end --\n");
  f.close();

  char drive_name[64];
  time_t now;
  time(&now);
  if (now > 1609459200UL) {
    struct tm t;
    gmtime_r(&now, &t);
    char tsbuf[32];
    strftime(tsbuf, sizeof(tsbuf), "%Y_%m_%d_%H_%M_%S", &t);
    snprintf(drive_name, sizeof(drive_name), "%s_%d_crash_hmi.log", tsbuf,
             (int)in3.serialNumber);
  } else {
    snprintf(drive_name, sizeof(drive_name), "boot_%d_crash_hmi_%lu.log",
             (int)in3.serialNumber, (unsigned long)hmi_crash_start_ms);
  }

  if (!driveEnqueueLogUpload(path, drive_name)) {
    logDrive("HMI crash enqueue failed");
  } else {
    logDrive(String("HMI crash captured (") + hmi_crash_len + " bytes) -> " +
             drive_name);
  }

  hmi_crash_capturing = false;
  hmi_crash_len       = 0;
}

// Returns true when the line was consumed by the crash FSM and must not be
// processed further as a protocol frame.
static bool hmiCrashAppend(const char *line) {
  uint32_t now = millis();

  if (hmi_crash_capturing && now - hmi_crash_last_ms > HMI_CRASH_TIMEOUT_MS) {
    // Stream stalled — flush what we have before re-evaluating this line.
    hmiCrashFlush();
  }

  if (!hmi_crash_capturing) {
    if (!lineStartsCrash(line))
      return false;
    hmi_crash_capturing = true;
    hmi_crash_len       = 0;
    hmi_crash_start_ms  = now;
    logDrive(String("HMI crash capture armed: ") + line);
  }

  size_t n = strlen(line);
  if (hmi_crash_len + (int)n + 1 < HMI_CRASH_BUF_SIZE) {
    memcpy(hmi_crash_buf + hmi_crash_len, line, n);
    hmi_crash_len += n;
    hmi_crash_buf[hmi_crash_len++] = '\n';
  }
  hmi_crash_last_ms = now;

  if (lineEndsCrash(line)) {
    hmiCrashFlush();
  }
  return true;
}

// ======================================================
//  LINE PARSER (common to UART and USB)
// ======================================================
void parse_line(const char *line) {
  // HMI panic output arrives as plain ROM/ESP_LOG text, so intercept it
  // before the protocol-prefix filter discards everything without "HMI,".
  if (hmiCrashAppend(line))
    return;

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
      if (strcmp(param, "FAN_SUPPLY_PWM") == 0) {
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
      } else if (strcmp(param, "FAN_CTL_PWM") == 0) {
        in3.fanCtlPWM = (int)value;
        EEPROM.writeInt(EEPROM_FAN_CTL_PWM, in3.fanCtlPWM);
        ledcWrite(FAN_CTL_PWM_CHANNEL, in3.fanCtlPWM);
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
        ESP_LOGI(TAG, "FAN_SUPPLY_PWM:%d FAN_CTL_PWM:%d HEATER_AMPS:%.2f SKIN_TMAX:%.2f AIR_TMAX:%.2f",
                 in3.fanPWM, in3.fanCtlPWM, in3.heaterMaxPowerAmps, in3.skinTemperatureSetMax,
                 in3.airTemperatureSetMax);
        xSemaphoreGiveRecursive(log_mutex);
      }
    }
    return;
  }

  if (strncmp(line, "HMI,", 4) == 0) {
    int act, skinE, mode, photo, mute, lang, photoMin;
    double air, skin, hum;
    int babyWeight = 0, babyGest = 0, babyAgeH = 0;

    int parsed = sscanf(line,
                        "HMI,%d,%d,%d,%lf,%lf,%lf,%d,%d,%d,%d,%d,%d,%d",
                        &act, &skinE, &mode, &air, &skin, &hum, &photo,
                        &mute, &lang, &photoMin,
                        &babyWeight, &babyGest, &babyAgeH);
    if (parsed >= 9) {
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
      if (parsed >= 13 && babyWeight > 0 && babyGest > 0) {
        bool changed = (hmi_cmd_msg.babyWeightGrams != babyWeight ||
                        hmi_cmd_msg.babyGestWeeks   != babyGest   ||
                        hmi_cmd_msg.babyAgeHours    != babyAgeH);
        hmi_cmd_msg.babyWeightGrams = babyWeight;
        hmi_cmd_msg.babyGestWeeks   = babyGest;
        hmi_cmd_msg.babyAgeHours    = babyAgeH;
        if (changed) hmi_cmd_msg.newBabyData = true;
      }
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
  // Si ya hay un cierre en curso o señalado, no iniciar nuevos URBs: cualquier
  // TX en flight mientras se llama a vcp->close() dispara assert en hcd_dwc.
  if (vcp_disconnecting)
    return;

  if (xSemaphoreTake(vcp_mux, pdMS_TO_TICKS(100)) != pdTRUE)
    return;

  if (!vcp || vcp_disconnecting) {
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
  if (err != ESP_OK) {
    // Marca ya dentro del mutex para que cualquier caller concurrente que
    // esté bloqueado tomándolo salga sin intentar TX.
    vcp_disconnecting = true;
  }
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
  uint32_t last_ppg_time = 0;
  // PPG normalisation state: decaying min/max keeps signal filling 0–255
  float ppg_min = -1.0f, ppg_max = 1.0f;
  // HR hysteresis: show after 2 consecutive valid samples, hide after 3 bad ones
  uint8_t hr_valid_streak = 0;
  uint8_t hr_bad_streak   = 0;
  bool    hr_displaying   = false;
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

    // --- PPG waveform (25 Hz = every 40 ms) ---
    if (millis() - last_ppg_time >= 40) {
      // No valid signal: reset normalisation and send flat midpoint so the
      // display collapses its amplitude window immediately.
      if (g_spo2_data.spo2_sqi < 0.05f) {
        ppg_min = -1.0f;
        ppg_max =  1.0f;
        hmiSerial.print("CTRL,PPG,128\n");
      } else {
        float ppg_raw = g_spo2_data.ppg;
        // Expand range immediately, decay slowly toward 0 (bandpass signal is zero-mean)
        if (ppg_raw < ppg_min) ppg_min = ppg_raw;
        else ppg_min += (0.0f - ppg_min) * 0.005f;
        if (ppg_raw > ppg_max) ppg_max = ppg_raw;
        else ppg_max += (0.0f - ppg_max) * 0.005f;
        float range = ppg_max - ppg_min;
        uint8_t ppg_byte = (range > 1e-3f)
            ? (uint8_t)constrain((ppg_raw - ppg_min) / range * 255.0f, 0.0f, 255.0f)
            : 128;
        char ppg_msg[16];
        snprintf(ppg_msg, sizeof(ppg_msg), "CTRL,PPG,%u\n", ppg_byte);
        hmiSerial.print(ppg_msg);
      }
      last_ppg_time = millis();
    }

    // --- Periodic telemetry + vitals (every 1 s) ---
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

      // HR fusion: weighted average of HR2+HR3 with agreement check and hysteresis
      uint8_t hr_byte = 0;
      {
        float h2 = g_spo2_data.hr2, h3 = g_spo2_data.hr3;
        float s2 = g_spo2_data.hr2_sqi, s3 = g_spo2_data.hr3_sqi;
        bool valid = (h2 > 0.0f) && (h3 > 0.0f) &&
                     (fabsf(h2 - h3) < 8.0f) &&
                     (fmaxf(s2, s3) >= 0.8f) &&
                     (fminf(s2, s3) >= 0.5f);
        if (valid) {
          hr_bad_streak = 0;
          if (++hr_valid_streak >= 2) hr_displaying = true;
        } else {
          hr_valid_streak = 0;
          if (++hr_bad_streak >= 3) hr_displaying = false;
        }
        if (hr_displaying && valid) {
          float hr_fused = (h2 * s2 + h3 * s3) / (s2 + s3);
          if (hr_fused >= 40.0f && hr_fused <= 240.0f)
            hr_byte = (uint8_t)(hr_fused + 0.5f);
        }
      }
      char vit_msg[20];
      snprintf(vit_msg, sizeof(vit_msg), "CTRL,VIT,%u,0\n", hr_byte);
      hmiSerial.print(vit_msg);

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
      vcp_disconnecting = false;
      // Drena posible señal pendiente de una sesión anterior.
      xSemaphoreTake(device_disconnected_sem, 0);
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
    uint32_t last_ppg_time = 0;
    float ppg_min = -1.0f, ppg_max = 1.0f;
    uint8_t hr_valid_streak = 0, hr_bad_streak = 0;
    bool hr_displaying = false;

    while (true) {
      if (xSemaphoreTake(hmi_state_req_sem, 0) == pdTRUE) {
        send_state_to_hmi();
      }

      // PPG waveform at 25 Hz
      if (millis() - last_ppg_time >= 40) {
        char ppg_msg[16];
        if (g_spo2_data.spo2_sqi < 0.05f) {
          ppg_min = -1.0f;
          ppg_max =  1.0f;
          snprintf(ppg_msg, sizeof(ppg_msg), "CTRL,PPG,128\n");
        } else {
          float ppg_raw = g_spo2_data.ppg;
          if (ppg_raw < ppg_min) ppg_min = ppg_raw;
          else ppg_min += (0.0f - ppg_min) * 0.005f;
          if (ppg_raw > ppg_max) ppg_max = ppg_raw;
          else ppg_max += (0.0f - ppg_max) * 0.005f;
          float range = ppg_max - ppg_min;
          uint8_t ppg_byte = (range > 1e-3f)
              ? (uint8_t)constrain((ppg_raw - ppg_min) / range * 255.0f, 0.0f, 255.0f)
              : 128;
          snprintf(ppg_msg, sizeof(ppg_msg), "CTRL,PPG,%u\n", ppg_byte);
        }
        CommunicationHost_Send(ppg_msg);
        last_ppg_time = millis();
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
        CommunicationHost_Send(msg);

        // HR fusion with hysteresis (same logic as UART path)
        uint8_t hr_byte = 0;
        {
          float h2 = g_spo2_data.hr2, h3 = g_spo2_data.hr3;
          float s2 = g_spo2_data.hr2_sqi, s3 = g_spo2_data.hr3_sqi;
          bool valid = (h2 > 0.0f) && (h3 > 0.0f) &&
                       (fabsf(h2 - h3) < 8.0f) &&
                       (fmaxf(s2, s3) >= 0.8f) &&
                       (fminf(s2, s3) >= 0.5f);
          if (valid) {
            hr_bad_streak = 0;
            if (++hr_valid_streak >= 2) hr_displaying = true;
          } else {
            hr_valid_streak = 0;
            if (++hr_bad_streak >= 3) hr_displaying = false;
          }
          if (hr_displaying && valid) {
            float hr_fused = (h2 * s2 + h3 * s3) / (s2 + s3);
            if (hr_fused >= 40.0f && hr_fused <= 240.0f)
              hr_byte = (uint8_t)(hr_fused + 0.5f);
          }
        }
        char vit_msg[20];
        snprintf(vit_msg, sizeof(vit_msg), "CTRL,VIT,%u,0\n", hr_byte);
        CommunicationHost_Send(vit_msg);

        last_tel_time = millis();
      }

      if (xSemaphoreTake(device_disconnected_sem, pdMS_TO_TICKS(10)) == pdTRUE) {
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

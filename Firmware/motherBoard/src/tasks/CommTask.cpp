#include "CommTask.h"
#include "main.h"
#include "modules/util/tz_source.h"
#include "tasks/PID.h"
#include "DriveUpload.h"
#include <LittleFS.h>
#include <Preferences.h>

#include "alarm_text.h"
#include "modules/control/alarm_history.h"
#include "modules/control/alarm_machine.h"
#include "modules/control/alarm_test.h"
#include "modules/baby_profile/baby_profile_protocol.h"
#include "modules/baby_profile/baby_profile_store.h"
#include "nte_table.h"
#include "alarm_policy.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>
#include <time.h>

static const char *TAG __attribute__((unused)) = "COMM_HOST";
extern SemaphoreHandle_t log_mutex;
extern char pendingSSID[64];
extern char pendingPass[64];
extern IncuNest_parameters in3;
extern double HeaterPIDOutput;
extern double humidityControlPIDOutput;
extern int    humidifierTimeCycle;

// ======================================================
//  GLOBAL DATA
// ======================================================
TelemetryMessage ctrl_tel_msg = {0, 0, 0, 0};
HMI_CommandMessage hmi_cmd_msg = {0, 0, 0, 0, 0, 0, 0, 0, false};
static HMI_CommandMessage g_last_cmd = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0, false};

static char rxBuffer[256];
static int rxIndex = 0;
static SemaphoreHandle_t hmi_state_req_sem;

static HardwareSerial &hmiSerial = Serial1;

// PPG transport scale: ADC counts of ppg_disp per LSB of the 0-255 display range.
// ppg_disp is zero-mean (library BPF output), so 128 is the baseline and this divisor
// sets the half-range to 127 x 16 = ~2030 counts. Measured on the bench 2026-08-13
// with HGAC active: peaks up to ~1350 counts at PI ~10%, so ~50% headroom.
// Fixed on purpose — no adaptive scaling outside the library.
static constexpr int32_t PPG_DISP_COUNTS_PER_LSB = 16;

// ---- Phototherapy Timer (Motherboard is source of truth) ----
static bool photoTimerActive = false;
static unsigned long photoTimerStartMs = 0;
static int photoTimerMinutes = 0;

// ======================================================
//  USB-ONLY HELPERS
// ======================================================
void parse_line(const char *line);

// ---- Baby-profile wizard flow state (one wizard at a time) ----
// seq selected by the current wizard (PROFILE_NEW/PROFILE_SELECT) and the
// weight answered for it (0 = SKIP). Used to compute CTRL,PROFILE_RANGE and
// to stamp activeSeq when control turns on.
static uint32_t s_wizardSeq = 0;
static uint16_t s_wizardGrams = 0;

// ---- Per-baby therapy-time accounting (phototherapy, thermo, humidity) ----
// All three counters share this: accumulate in RAM, flush to NVS only on the
// OFF edge or every THERAPY_FLUSH_INTERVAL_MS. A slot write per elapsed minute
// would wear the flash for no benefit.
static const uint32_t THERAPY_FLUSH_INTERVAL_MS = 10u * 60u * 1000u;

// Plain aggregate on purpose (no default member initialisers): those would
// make it non-aggregate and break the brace-init below on this toolchain.
struct TherapyAccumulator {
  bool wasOn;
  uint32_t sinceMs;   // start of the un-credited stretch
  uint32_t creditSeq; // baby the stretch belongs to
  bool (*commit)(uint32_t, uint32_t);
};

static TherapyAccumulator s_photoAcc{false, 0, 0,
                                     babyStore_addPhototherapyMinutes};
static TherapyAccumulator s_thermoAcc{false, 0, 0, babyStore_addThermoMinutes};
static TherapyAccumulator s_humidityAcc{false, 0, 0,
                                        babyStore_addHumidityMinutes};

// Credits whole elapsed minutes and rebases, keeping the sub-minute
// remainder so repeated flushes never round it away.
static void flushTherapy(TherapyAccumulator &a) {
  if (a.creditSeq == 0) return;
  uint32_t minutes = (millis() - a.sinceMs) / 60000u;
  if (minutes == 0) return;
  a.commit(a.creditSeq, minutes);
  a.sinceMs += minutes * 60000u;
}

static void updateTherapyAccounting(TherapyAccumulator &a, bool on) {
  if (on && !a.wasOn) {
    a.creditSeq = babyStore_getActiveSeq();
    a.sinceMs = millis();
  } else if (!on && a.wasOn) {
    flushTherapy(a);
    a.creditSeq = 0;
  } else if (on && millis() - a.sinceMs >= THERAPY_FLUSH_INTERVAL_MS) {
    flushTherapy(a);
  }
  a.wasOn = on;
}

// Builds and sends CTRL,PROFILE_RANGE for the current wizard flow.
static void sendProfileRange(uint32_t seq, bool ageKnown, uint16_t ageDays) {
  const BabyProfile *p = babyStore_findBySeq(seq);
  NteRange r = {-1.0f, -1.0f, -1.0f, true};
  if (p && s_wizardGrams > 0 && ageKnown) {
    r = calculateNteRange(s_wizardGrams, p->gestWeeks, ageDays);
  }
  char buf[96];
  if (baby_proto_build_range(buf, sizeof(buf), seq, ageKnown, ageDays, &r) >
      0) {
    hmiSerial.print(buf);
  }
}

// Handles one already-validated HMI,PROFILE_*/HMI,WEIGHT_HISTORY_REQ line.
// Malformed lines are discarded with an error log (never partial data).
// Response scratch buffers are static, NOT stack: COMM_TASK only has a 4 KB
// stack and the PROFILE_WEIGHT path is the deepest in this file (LittleFS
// write + float formatting in the range builder both need hundreds of bytes
// below this frame). A 1 KB local here was enough to run it out of stack.
// Safe: parse_line runs only on COMM_TASK and one wizard flow at a time.
// 1280: worst-case CTRL,PROFILE_HISTORY is 10 entries x 10 fields plus the
// name, with every field at max width (~1110 chars). Sized with margin so a
// full page can never silently fail to build. The HMI's COMM_RX_BUFFER_SIZE
// is the same 1280 — keep both in step if a field is ever added.
static char s_babyRespBuf[1280];
static BabyProfile s_babyHistPage[10];
static BabyWeightPoint s_babyWeightPts[BABY_WEIGHT_HISTORY_MAX_OUT];

static void handleBabyLine(const char *line) {
  BabyProtoMsg m;
  if (baby_proto_parse(line, &m) == BABY_MSG_NONE) {
    if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGE(TAG, "PROFILE line discarded (malformed)");
      xSemaphoreGiveRecursive(log_mutex);
    }
    return;
  }

  char *buf = s_babyRespBuf;
  const size_t bufLen = sizeof(s_babyRespBuf);
  switch (m.type) {
    case BABY_MSG_LIST_REQ: {
      BabyProfile slots[BABY_ACTIVE_SLOTS];
      babyStore_getSlots(slots);
      if (baby_proto_build_list(buf, bufLen, slots) > 0) {
        hmiSerial.print(buf);
      }
      break;
    }
    case BABY_MSG_NEW: {
      uint32_t seq = babyStore_createProfile(m.name, m.gestWeeks);
      if (seq != 0) {
        s_wizardSeq = seq;
        s_wizardGrams = 0;
      }
      snprintf(buf, bufLen, "CTRL,PROFILE_ACK,%u\n", (unsigned)seq);
      hmiSerial.print(buf);
      break;
    }
    case BABY_MSG_SELECT: {
      const BabyProfile *p = babyStore_findBySeq(m.seq);
      uint32_t ack = 0;
      if (p) {
        s_wizardSeq = m.seq;
        s_wizardGrams = 0;
        ack = m.seq;
      }
      snprintf(buf, bufLen, "CTRL,PROFILE_ACK,%u\n", (unsigned)ack);
      hmiSerial.print(buf);
      break;
    }
    case BABY_MSG_WEIGHT: {
      if (!babyStore_recordWeight(m.seq, m.grams)) break;  // unknown seq
      s_wizardSeq = m.seq;
      s_wizardGrams = m.grams;
      uint16_t ageDays = 0;
      bool ageKnown = babyStore_deriveAgeDays(m.seq, &ageDays);
      sendProfileRange(m.seq, ageKnown, ageDays);
      break;
    }
    case BABY_MSG_AGE_MANUAL: {
      if (m.seq != s_wizardSeq) break;  // stale/out-of-flow answer
      sendProfileRange(m.seq, true, m.ageDays);
      break;
    }
    case BABY_MSG_DISCHARGE: {
      bool ok = babyStore_discharge(m.seq, m.outcome);
      snprintf(buf, bufLen, "CTRL,PROFILE_ACK,%u\n",
               ok ? (unsigned)m.seq : 0u);
      hmiSerial.print(buf);
      break;
    }
    case BABY_MSG_KANGAROO: {
      // Baby out with the mother: counted, never archived, and activeSeq is
      // deliberately left alone so the profile keeps its FIFO protection
      // while it is out of the incubator.
      bool ok = babyStore_recordKangaroo(m.seq);
      snprintf(buf, bufLen, "CTRL,PROFILE_ACK,%u\n",
               ok ? (unsigned)m.seq : 0u);
      hmiSerial.print(buf);
      break;
    }
    case BABY_MSG_HISTORY_REQ: {
      uint32_t total = 0;
      uint32_t n = babyStore_readHistoryPage(m.page, 10, s_babyHistPage, &total);
      if (baby_proto_build_history(buf, bufLen, m.page, total, s_babyHistPage,
                                   n) > 0) {
        hmiSerial.print(buf);
      }
      break;
    }
    case BABY_MSG_WEIGHT_HISTORY_REQ: {
      uint32_t n = babyStore_readWeightHistory(m.seq, s_babyWeightPts,
                                               BABY_WEIGHT_HISTORY_MAX_OUT);
      if (baby_proto_build_weight_history(buf, bufLen, m.seq, s_babyWeightPts, n) >
          0) {
        hmiSerial.print(buf);
      }
      break;
    }
    default:
      break;
  }

  // Diagnostic: this task's worst-case free stack. The baby-profile paths are
  // the deepest in COMM_TASK, so if headroom is ever going to run out it will
  // show here first. Logged as a warning below 1 KB so it is impossible to
  // miss on the serial monitor.
  UBaseType_t freeStack = uxTaskGetStackHighWaterMark(nullptr);
  if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (freeStack < 1024) {
      ESP_LOGW(TAG, "PROFILE msg=%d handled, COMM_TASK stack free=%u B (LOW)",
               (int)m.type, (unsigned)freeStack);
    } else {
      ESP_LOGI(TAG, "PROFILE msg=%d handled, COMM_TASK stack free=%u B",
               (int)m.type, (unsigned)freeStack);
    }
    xSemaphoreGiveRecursive(log_mutex);
  }
}

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
      { Preferences p; p.begin("photo", false); p.clear(); p.end(); }

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
  // 192 y no 160: el campo silencedBitmask anadio hasta 11 caracteres y el
  // peor caso (fwVer de 20, tres bitmask/contadores al ancho maximo) se
  // quedaba al filo. Truncar aqui no da error, da una linea que el HMI
  // descarta entera y una pantalla que deja de refrescarse.
  char msg[192];
  int alarmCount = getActiveAlarmCount();
  double remainingTime = getRemainingPhotoTime();

  // Un bit por AlarmId, tal cual lo produce la maquina de alarmas.
  const uint32_t alarmBitmask = alarm_machine_bitmask();

  // Estado REAL del audio, no el eco del comando del display.
  //
  // Este campo devolvia g_last_cmd.muteAlarm, es decir, lo ultimo que el
  // propio HMI habia pedido: no aportaba nada y ademas mentia. La pausa de
  // audio caduca sola a los 2 min (6.8.3) y el zumbador vuelve; el display se
  // quedaba creyendo que seguia silenciada y no volvia a ofrecer el boton de
  // silencio, dejando al operador con una alarma sonando que ya no podia
  // callar. Ahora se manda si queda algo silenciable, que es la pregunta que
  // el display necesita responder.
  const int audioSilenced =
      (alarmBitmask != 0 && !alarm_machine_any_silenceable()) ? 1 : 0;

  // Que condiciones concretas estan en AUDIO PAUSED. 6.8.1 exige que el
  // operador pueda determinar CUALES estan inactivadas, no solo que "algo" lo
  // esta, y 201.12.3.104 que cada alarma silenciada mantenga su indicacion
  // visual. Con un booleano global eso es indistinguible.
  const uint32_t silencedBitmask = alarm_machine_silenced_bitmask();

  // Prioridad que reproduce la prueba de funcionamiento, o ALARM_TEST_IDLE.
  // El display la necesita para pintar el banner con el color y el parpadeo de
  // esa prioridad: sin eso la prueba solo verificaria el zumbador, y
  // 201.12.3.105 pide comprobar las alarmas "audible AND visual".
  const int almTest = (int)alarm_test_priority();

  // Segundos que faltan para que vuelva el audio de la pausa que expira
  // antes, o 0 si no hay ninguna silenciada. El display lo pinta junto al
  // icono de AUDIO PAUSED: la norma recomienda esa cuenta atras justamente
  // para distinguir el estado TEMPORIZADO del permanente (racional de 6.8.5).
  const int silenceLeftS =
      (int)((alarm_machine_silence_remaining_ms(millis()) + 999u) / 1000u);

  // Derive probe state from skin temperature: >0.1°C means probe is physically connected
  int skinProbeState = (in3.temperature[SKIN_SENSOR] > 0.1f) ? SKIN_PROBE_VALID
                                                              : SKIN_PROBE_NOT_CONNECTED;

  snprintf(msg, sizeof(msg),
           "CTRL,STATE,%d,%d,%.2f,%.2f,%.0f,%d,%d,%d,%d,%c,%s,%d,%d,%d,%.2f,%d,%d,0x%X,0x%X,%d,%d\n",
           (int)g_last_cmd.actuation, (int)g_last_cmd.controlMode,
           (double)g_last_cmd.desiredAirTemperature,
           (double)g_last_cmd.desiredSkinTemperature,
           (double)g_last_cmd.desiredHumidity, (int)g_last_cmd.phototherapyMode,
           audioSilenced, ctrl_tel_msg.serialNumber, HW_NUM,
           HW_REVISION, FWversion, alarmCount, (int)g_last_cmd.skinModeEnabled,
           (int)ctrl_tel_msg.serverCommStatus, remainingTime, in3.language,
           skinProbeState, alarmBitmask, silencedBitmask, almTest,
           silenceLeftS);


  ESP_LOGI(TAG, "Sending state to HMI: %s", msg);
  CommunicationHost_Send(msg);

  if (alarmCount > 0) {
    resendActiveAlarms();
  }
}

// ======================================================
//  SEND WIFI CREDENTIALS TO HMI
// ======================================================
void sendWifiToHMI(const char* ssid, const char* pass) {
  char msg[160];
  snprintf(msg, sizeof(msg), "CTRL,WIFI,%s,%s\n", ssid, pass);
  CommunicationHost_Send(msg);
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
// Instante de la ultima linea valida recibida del display, y si se ha visto
// alguna vez. Los lee checkHmiLink() (security.cpp).
//
// g_hmiEverSeen existe porque sin el, el arranque declararia el enlace perdido
// durante los primeros segundos: la placa arranca antes que el display y su
// primera trama tarda en llegar. Un enlace que nunca ha existido no es un
// enlace caido.
uint32_t g_lastHmiLineMs = 0;
bool g_hmiEverSeen = false;

void parse_line(const char *line) {
  // HMI panic output arrives as plain ROM/ESP_LOG text, so intercept it
  // before the protocol-prefix filter discards everything without "HMI,".
  if (hmiCrashAppend(line))
    return;

  if (strncmp(line, EXPECTED_PREFIX, strlen(EXPECTED_PREFIX)) != 0) {
    return;
  }

  // Latido del display. Se marca AQUI, con el prefijo ya validado y antes de
  // cualquier rama: sirve igual una trama de estado que un comando suelto, y
  // ponerlo en cada rama se olvidaria en la siguiente que se anada. Lo que se
  // vigila es que el display siga HABLANDO, no lo que diga.
  g_lastHmiLineMs = millis();
  g_hmiEverSeen = true;

  if (strncmp(line, "HMI,BOOT,", 9) == 0) {
    int rst = 0, cnt = 0;
    if (sscanf(line, "HMI,BOOT,%d,%d", &rst, &cnt) == 2) {
      g_hmiLastRst = rst;
      g_hmiBootCount = cnt;
      if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGW(TAG, "[DIAG] HMI boot count=%d lastRst=%d", cnt, rst);
        xSemaphoreGiveRecursive(log_mutex);
      }
    }
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
      if (strcmp(param, "BABY_WIPE") == 0) {
        // Destructive and irreversible, so it needs an explicit magic value
        // rather than any truthy number: /config,BABY_WIPE,1234
        if ((int)value == 1234) {
          int n = babyStore_wipeAll();
          if (xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGW(TAG, "BABY_WIPE done, %d files removed", n);
            xSemaphoreGiveRecursive(log_mutex);
          }
        } else {
          success = false;  // wrong confirmation code: do nothing
        }
      } else if (strcmp(param, "FAN_SUPPLY_PWM") == 0) {
        in3.fanPwrSupplyPWM = (int)value;
        { Preferences p; p.begin(NS_CFG, false); p.putInt(KEY_FAN_PWR_SUPPLY_PWM, in3.fanPwrSupplyPWM); p.end(); }
      } else if (strcmp(param, "HEATER_AMPS") == 0) {
        in3.heaterMaxPowerAmps = value;
        { Preferences p; p.begin(NS_CFG, false); p.putFloat(KEY_HEAT_MAX_A, in3.heaterMaxPowerAmps); p.end(); }
      } else if (strcmp(param, "SKIN_TMAX") == 0) {
        in3.skinTemperatureSetMax = alarm_clamp_skin_cutout(value);
        maxDesiredTemp[CONTROL_SKIN] = in3.skinTemperatureSetMax;
        { Preferences p; p.begin(NS_CFG, false); p.putFloat(KEY_SKIN_T_MAX, in3.skinTemperatureSetMax); p.end(); }
      } else if (strcmp(param, "AIR_TMAX") == 0) {
        in3.airTemperatureSetMax = alarm_clamp_air_cutout(value);
        maxDesiredTemp[CONTROL_AIR] = in3.airTemperatureSetMax;
        { Preferences p; p.begin(NS_CFG, false); p.putFloat(KEY_AIR_T_MAX, in3.airTemperatureSetMax); p.end(); }
      } else if (strcmp(param, "GPRS_ACT") == 0) {
        in3.actuating_gprs_period = (int)value;
        { Preferences p; p.begin(NS_GPRS, false); p.putInt(KEY_ACT_PERIOD, in3.actuating_gprs_period); p.end(); }
      } else if (strcmp(param, "GPRS_PHOTO") == 0) {
        in3.phototherapy_gprs_period = (int)value;
        { Preferences p; p.begin(NS_GPRS, false); p.putInt(KEY_PHOTO_PERIOD, in3.phototherapy_gprs_period); p.end(); }
      } else if (strcmp(param, "GPRS_STBY") == 0) {
        in3.standby_gprs_period = (int)value;
        { Preferences p; p.begin(NS_GPRS, false); p.putInt(KEY_STBY_PERIOD, in3.standby_gprs_period); p.end(); }
      } else if (strcmp(param, "FAN_CTL_PWM") == 0) {
        in3.fanCtlPWM = (int)value;
        { Preferences p; p.begin(NS_CFG, false); p.putInt(KEY_FAN_CTL_PWM, in3.fanCtlPWM); p.end(); }
        ledcWrite(FAN_CTL_PWM_CHANNEL, in3.fanCtlPWM);
      } else if (strcmp(param, "FAN_PID_EN") == 0) {
        setFanPidEnabled(value != 0);
      } else {
        success = false;
      }
      if (success) { /* Preferences commits on p.end() */ }
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
                 in3.fanPwrSupplyPWM, in3.fanCtlPWM, in3.heaterMaxPowerAmps, in3.skinTemperatureSetMax,
                 in3.airTemperatureSetMax);
        xSemaphoreGiveRecursive(log_mutex);
      }
    }
    return;
  }

  if (strncmp(line, "HMI,PROFILE_", 12) == 0 ||
      strncmp(line, "HMI,WEIGHT_HISTORY_REQ", 22) == 0) {
    handleBabyLine(line);
    return;
  }

  // Registro de alarmas (IEC 60601-1-8 6.12.2). Solo bajo demanda, nunca en la
  // telemetria periodica: known_issues.md #2 documenta una inundacion de UART
  // real por trafico periodico evitable.
  //
  // El titulo viaja RESUELTO, no el id para que el display lo traduzca: la
  // motherBoard es la dueña de la informacion y el display se limita a
  // pintarla. Cuesta linea, pero mantiene toda la logica de alarmas —
  // identidad, prioridad e idioma — en un solo lado.
  if (strncmp(line, "HMI,ALM_HISTORY_REQ", 19) == 0) {
    char msg[ALARM_HISTORY_LINE_BUF_SIZE];
    const Language lang = (Language)in3.language;
    const size_t n = alarm_history_count();
    int off = snprintf(msg, sizeof(msg), "CTRL,ALM_HISTORY,%u", (unsigned)n);
    for (size_t i = 0; i < n && off > 0 && off < (int)sizeof(msg); ++i) {
      const AlarmHistoryEntry *e = alarm_history_get(i);
      if (!e) {
        break;
      }
      off += snprintf(msg + off, sizeof(msg) - off,
                      ",%u,%u,%u,%lu,%lu,%d,%d,%.*s", (unsigned)e->id,
                      (unsigned)e->priority, (unsigned)e->resolved,
                      (unsigned long)e->raisedEpoch,
                      (unsigned long)e->clearedEpoch, (int)e->limitCenti,
                      (int)e->valueCenti, ALARM_TITLE_MAX_CHARS,
                      alarm_title_text((AlarmId)e->id, lang));
    }
    if (off > 0 && off < (int)sizeof(msg)) {
      snprintf(msg + off, sizeof(msg) - off, "\n");
      CommunicationHost_Send(msg);
    } else {
      // Truncada: mandar una linea a medias es peor que no mandarla, porque el
      // display la parsearia parcialmente. Se descarta con log, como el resto
      // del protocolo (.claude/rules/security.md).
      logE("[ALARM] historial no cabe en la linea, no se envia");
    }
    return;
  }

  // Prueba de funcionamiento de las senales de alarma (201.12.3.105).
  //
  // Se rechaza si hay CUALQUIER condicion senalizando. No es una comodidad:
  // arrancar una secuencia de prueba con una alarma real en curso pisaria el
  // patron de esa alarma con otro distinto, que es lo contrario de lo que
  // busca 6.3.2.2.2 — el operador dejaria de poder identificar que suena.
  if (strncmp(line, "HMI,ALM_TEST", 12) == 0) {
    if (alarm_machine_any_signalling()) {
      // logAlarm: logE esta compilado a nada (LOG_ERRORS false en main.h), asi
      // que este rechazo llevaba siendo invisible desde que se escribio.
      logAlarm("[ALARM] prueba rechazada: bitmask=0x" +
               String(alarm_machine_bitmask(), HEX));
      return;
    }
    alarm_test_start(millis());
    logAlarm("[ALARM] prueba de funcionamiento arrancada");
    return;
  }

  // Silencio / cancelacion de silencio de UNA condicion.
  //
  // 6.8.1 permite que la inactivacion se aplique a una condicion individual y
  // exige que no afecte a las demas, que es justo lo que hace
  // alarm_machine_silence(). 6.8.4 exige poder TERMINAR cualquier estado de
  // inactivacion: eso es on=0.
  if (strncmp(line, "HMI,ALM_SILENCE,", 16) == 0) {
    unsigned id = 0;
    int on = 0;
    if (sscanf(line + 16, "%u,%d", &id, &on) != 2 || id >= ALARM_COUNT) {
      logE("[ALARM] ALM_SILENCE malformada, descartada");
      return;
    }
    const uint32_t now = millis();
    if (on) {
      alarm_machine_silence((AlarmId)id, ALARM_AUDIO_PAUSE_MS, now);
    } else {
      alarm_machine_unsilence((AlarmId)id, now);
    }
    // Se deja registrado: silenciar es una accion del operador sobre una
    // alarma y conviene que quede rastro. logAlarm y no logI/logE porque esas
    // dos estan compiladas a nada (LOG_INFORMATION/LOG_ERRORS a false en
    // main.h) — y es ademas la categoria que corresponde.
    logAlarm("[ALARM] ALM_SILENCE id=" + String(id) + " on=" + String(on) +
             " -> estado=" + String((int)alarm_machine_state((AlarmId)id)));
    return;
  }

  // Descripcion de una alarma concreta, bajo demanda.
  //
  // No viaja dentro de CTRL,ALM_HISTORY porque no cabe: 10 entradas x (29 de
  // titulo + 99 de descripcion + campos) desborda ALARM_HISTORY_LINE_BUF_SIZE,
  // y una linea truncada se descarta entera. El display la pide solo cuando el
  // operador abre el detalle de una entrada, que es una accion humana y
  // esporadica — sin riesgo de inundar la UART (known_issues.md #2).
  //
  // Sigue siendo la placa quien resuelve texto e idioma: el display manda un
  // id y recibe el par titulo/descripcion ya listo para pintar.
  if (strncmp(line, "HMI,ALM_DESC_REQ,", 17) == 0) {
    unsigned id = 0;
    if (sscanf(line + 17, "%u", &id) != 1 || id >= ALARM_COUNT) {
      logE("[ALARM] ALM_DESC_REQ con id invalido, descartada");
      return;
    }
    const Language lang = (Language)in3.language;
    char msg[ALARM_LINE_BUF_SIZE];
    snprintf(msg, sizeof(msg), "CTRL,ALM_DESC,%u,%.*s,%.*s\n", id,
             ALARM_TITLE_MAX_CHARS, alarm_title_text((AlarmId)id, lang),
             ALARM_DESC_MAX_CHARS, alarm_action_text((AlarmId)id, lang));
    CommunicationHost_Send(msg);
    return;
  }

  if (strncmp(line, "HMI,", 4) == 0) {
    int act, skinE, mode, photo, mute, lang, photoMin;
    double air, skin, hum;

    int parsed = sscanf(line,
                        "HMI,%d,%d,%d,%lf,%lf,%lf,%d,%d,%d,%d",
                        &act, &skinE, &mode, &air, &skin, &hum, &photo,
                        &mute, &lang, &photoMin);
    if (parsed >= 9) {
      // activeSeq protection: stamp the wizard-selected baby while ANY
      // therapy is running, and only clear it once every therapy is off.
      // Phototherapy counts: the HMI now runs the baby-data wizard for it
      // too, and a lamp-only session still has a baby in the incubator whose
      // profile must keep its FIFO protection (and receive the phototherapy
      // minutes accounted below).
      bool anyTherapyNow = (act != 0) || (photo != 0);
      bool anyTherapyBefore = (hmi_cmd_msg.actuation != 0) ||
                              (hmi_cmd_msg.phototherapyMode != 0);
      if (anyTherapyNow && !anyTherapyBefore && s_wizardSeq != 0) {
        babyStore_setActiveSeq(s_wizardSeq);
      } else if (!anyTherapyNow && anyTherapyBefore) {
        babyStore_setActiveSeq(0);
      }

      // Per-baby therapy exposure. Accumulated in RAM and only written to NVS
      // on the OFF edge or every THERAPY_FLUSH_INTERVAL — a slot write per
      // minute would wear the flash for no benefit.
      updateTherapyAccounting(s_photoAcc, photo != 0);
      updateTherapyAccounting(s_thermoAcc,
                              act == ACTUATION_TEMPERATURE ||
                                  act == ACTUATION_TEMP_AND_HUMIDITY);
      updateTherapyAccounting(s_humidityAcc,
                              act == ACTUATION_HUMIDITY ||
                                  act == ACTUATION_TEMP_AND_HUMIDITY);

      // El display mantiene muteAlarm a nivel alto mientras el operador siga
      // en estado "muteado" (se lee desde alarmsMuted en su UITask, no un
      // pulso), y la trama HMI llega periodicamente: disparar en cada ciclo
      // reiniciaria la ventana de silencio (ALARM_AUDIO_PAUSE_MS,
      // security.cpp) eternamente. Por eso se detecta el flanco de subida
      // contra el valor previo, no el nivel.
      bool muteRisingEdge = (hmi_cmd_msg.muteAlarm == 0) && (mute != 0);

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

      if (muteRisingEdge) {
        silenceActiveAlarmsFromDisplayMute();
      }

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

// ¿Se sigue leyendo este sensor?
//
// Misma fuente y misma ventana que checkStatusOfSensor() (security.cpp) usa
// para levantar las alarmas de fallo de sensor: si se separasen, el display
// podria pintar una cifra mientras la alarma dice que ese sensor no responde.
//
// lastSuccesfullSensorUpdate == 0 es "no se ha leido NUNCA" (el centinela que
// usa applyNTCResult), y cuenta como no disponible: es el caso de la sonda de
// piel ausente en modo aire, donde lo honesto es un "--" y no un 0.0.
static const uint32_t TEL_SENSOR_STALE_MS = 5000u;

static bool sensorReadingFresh(byte sensor) {
  extern long lastSuccesfullSensorUpdate[SENSOR_TEMP_QTY];
  if (lastSuccesfullSensorUpdate[sensor] == 0) return false;
  return (uint32_t)(millis() - (uint32_t)lastSuccesfullSensorUpdate[sensor]) <=
         TEL_SENSOR_STALE_MS;
}

// ======================================================
//  SEND DATA TO HMI
// ======================================================
void CommunicationHost_Send(const char *msg) {
  hmiSerial.print(msg);
}

// ======================================================
//  INITIALIZATION
// ======================================================
void CommunicationHost_Init() {
  hmi_state_req_sem = xSemaphoreCreateBinary();
  babyStore_init();

  hmiSerial.begin(115200, SERIAL_8N1, UART_MB_RX_PIN, UART_MB_TX_PIN);
  ESP_LOGI(TAG, "UART comm initialized on RX=%d TX=%d",
           UART_MB_RX_PIN, UART_MB_TX_PIN);
}

// ======================================================
//  COMMUNICATION TASK
// ======================================================
void Communication_Task(void *pvParameters) {
  // ---- UART path ----
  uint32_t last_tel_time = 0;
  // Force the first CTRL,TIME immediately rather than 10 s into the session.
  uint32_t last_time_bcast = (uint32_t)(0 - 10001);
  uint32_t last_ppg_time = 0;
  uint32_t ppg_time      = 0;
  static unsigned long last_probe_status_time = 0;
  static ProbeState    prev_probe_state       = ProbeState::PROBE_DISCONNECTED;
  // HR hysteresis: show after 2 consecutive valid samples, hide after 3 bad ones
  uint8_t hr_valid_streak = 0;
  uint8_t hr_bad_streak   = 0;
  bool    hr_displaying   = false;
  ESP_LOGI(TAG, "UART communication task started");

  if (g_restore_photo_minutes > 0) {
    photoTimerMinutes  = g_restore_photo_minutes;
    photoTimerActive   = true;
    photoTimerStartMs  = millis();
    g_restore_photo_minutes = 0;
    ESP_LOGW(TAG, "[RESTORE] photo timer resumed: %d min", photoTimerMinutes);
  }

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

    // Detect APPLIED transition — notify display immediately
    {
      ProbeState cur = g_spo2_data.probe_state;
      if (cur == ProbeState::PROBE_APPLIED && prev_probe_state != ProbeState::PROBE_APPLIED) {
        hmiSerial.print("CTRL,PROBE,2\n");
      }
      prev_probe_state = cur;
    }

    // --- STATE: periodico (1 Hz) y ademas bajo peticion ---
    //
    // Antes SOLO salia bajo peticion, y el display deja de pedirlo en cuanto
    // se sincroniza. Resultado: CTRL,STATE se emitia unas pocas veces al
    // arrancar y nunca mas, asi que todo lo que viaja en el se quedaba
    // congelado con el valor del arranque — silencedBitmask, la cuenta atras
    // de la pausa de audio y la prioridad de la prueba de alarmas incluidos.
    // El sintoma era que la campana de AUDIO PAUSED solo aparecia si ya habia
    // una alarma silenciada al encender, y que el temporizador nunca bajaba.
    //
    // PROTOCOL.md ya decia "Enviado cada 1 segundo o bajo peticion": era el
    // codigo el que no cumplia su propio contrato. 1 Hz es la misma cadencia
    // que CTRL,TEL, con el que se intercala.
    static uint32_t last_state_time = 0;
    const bool stateRequested =
        (xSemaphoreTake(hmi_state_req_sem, 0) == pdTRUE);
    if (stateRequested || (millis() - last_state_time >= 1000)) {
      send_state_to_hmi();
      last_state_time = millis();
    }

    // --- PPG waveform (25 Hz = every 40 ms) ---
    ppg_time = millis();
    if (ppg_time - last_ppg_time >= 40) {
      // Transport only: fixed scale, no state, no adaptive gain. The waveform is NOT
      // gated on spo2_sqi — the library restarts an 18 s SpO2 warmup on every probe
      // application (_spo2_update: EMAs reset while not APPLIED), while ppg_disp is
      // valid within ~1 s. Gating the trace on SpO2 validity flat-lined it for 19 s.
      if (g_spo2_data.probe_state == ProbeState::PROBE_APPLIED) {
        long ppg_scaled = 128L + (long)(g_spo2_data.ppg_disp / PPG_DISP_COUNTS_PER_LSB);
        uint8_t ppg_byte = (uint8_t)constrain(ppg_scaled, 0L, 255L);
        char ppg_msg[16];
        snprintf(ppg_msg, sizeof(ppg_msg), "CTRL,PPG,%u\n", ppg_byte);
        hmiSerial.print(ppg_msg);
      }
      last_ppg_time = ppg_time;
    }

    // --- Persist phototherapy timer to NVS every 60 s ---
    {
      static unsigned long last_photo_save = 0;
      if (photoTimerActive && millis() - last_photo_save > 60000) {
        long elapsed = (long)((millis() - photoTimerStartMs) / 1000);
        int remaining_mins = ((long)photoTimerMinutes * 60 - elapsed + 59) / 60;
        if (remaining_mins < 1) remaining_mins = 1;
        Preferences p;
        p.begin("photo", false);
        p.putBool("active", true);
        p.putInt("mins", remaining_mins);
        p.end();
        last_photo_save = millis();
      }
    }

    // --- Periodic telemetry + vitals (every 1 s) ---
    if (millis() - last_tel_time > 1000) {
      int status = COMM_STATUS_NONE;
      if (WIFIIsConnected()) {
        status = WIFIIsConnectedToServer() ? COMM_STATUS_WIFI_SERVER
                                           : COMM_STATUS_WIFI_ONLY;
      } else if (GPRSIsAttached()) {
        status = GPRSIsConnectedToServer() ? COMM_STATUS_GPRS_SERVER
                                           : COMM_STATUS_GPRS_ONLY;
      }
      ctrl_tel_msg.serverCommStatus = status;

      // Una medida que la placa ya no esta consiguiendo leer NO se manda como
      // 0: se manda como centinela de "no disponible" y el display la pinta
      // "--". Se decide aqui, en la via de presentacion, y NO tocando
      // in3.temperature[]: ese array alimenta el PID y las alarmas, y cambiar
      // su convencion para arreglar una etiqueta de pantalla seria meter mano
      // en el control de un actuador por un motivo cosmetico.
      //
      // Misma ventana de silencio que usa checkStatusOfSensor() para levantar
      // las alarmas de fallo de sensor, para que el "--" y la alarma aparezcan
      // a la vez y no se contradigan en pantalla.
      const double telAir =
          sensorReadingFresh(ROOM_DIGITAL_TEMP_SENSOR)
              ? ctrl_tel_msg.detectedAirTemperature
              : PROTO_TEL_TEMP_UNAVAILABLE;
      const double telSkin =
          sensorReadingFresh(SKIN_SENSOR)
              ? ctrl_tel_msg.detectedSkinTemperature
              : PROTO_TEL_TEMP_UNAVAILABLE;
      // La humedad y la temperatura de aire vienen del MISMO sensor digital de
      // camara, asi que su frescura es la misma medida.
      const int telHum = sensorReadingFresh(ROOM_DIGITAL_TEMP_SENSOR)
                             ? (int)ctrl_tel_msg.detectedHumidity
                             : PROTO_TEL_HUM_UNAVAILABLE;

      char msg[64];
      snprintf(msg, sizeof(msg), "CTRL,TEL,%.1f,%.1f,%d,%d\n", telAir, telSkin,
               telHum, ctrl_tel_msg.serverCommStatus);
      hmiSerial.print(msg);

      {
        // Mirrors the same 0..PWM_MAX_VALUE scale and ongoingCriticalAlarm()
        // gating that PIDHandler() actually writes via ledcWrite(), so the
        // HMI bar never disagrees with the log or with the real hardware duty.
        int temp_duty = ongoingCriticalAlarm() ? 0 : (int)(HeaterPIDOutput + 0.5);
        if (temp_duty < 0)               temp_duty = 0;
        if (temp_duty > PWM_MAX_VALUE)   temp_duty = PWM_MAX_VALUE;

        int hum_duty = (humidifierTimeCycle > 0)
            ? (int)(humidityControlPIDOutput / humidifierTimeCycle * PWM_MAX_VALUE + 0.5)
            : 0;
        if (hum_duty < 0)             hum_duty = 0;
        if (hum_duty > PWM_MAX_VALUE) hum_duty = PWM_MAX_VALUE;

        char duty_msg[DUTY_MSG_BUF_SIZE];
        snprintf(duty_msg, sizeof(duty_msg), "CTRL,DUTY,%d,%d\n", temp_duty, hum_duty);
        hmiSerial.print(duty_msg);
      }

      if (g_spo2_data.probe_state != ProbeState::PROBE_APPLIED) {
        // Probe not on patient — send status every 2 s, suppress vitals
        if (millis() - last_probe_status_time >= 2000) {
          char probe_msg[20];
          snprintf(probe_msg, sizeof(probe_msg), "CTRL,PROBE,%u\n",
                   (uint8_t)g_spo2_data.probe_state);
          hmiSerial.print(probe_msg);
          last_probe_status_time = millis();
        }
      } else {
        // Probe applied — send fused HR vitals
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
        float pi_val = g_spo2_data.pi;
        char vit_msg[32];
        snprintf(vit_msg, sizeof(vit_msg), "CTRL,VIT,%u,0,%.2f\n",
                 hr_byte, pi_val);
        hmiSerial.print(vit_msg);
      }

      last_tel_time = millis();
    }

    // Wall-clock broadcast. The HMI has no clock of its own (no RTC, no NTP),
    // so the motherBoard — the only board that syncs time, over WiFi — is the
    // single source. Every 10 s rather than with the 1 Hz block: the HMI only
    // needs it to render dates, and known_issues #2 warns against adding
    // avoidable periodic UART traffic. epoch 0 means "not synced yet".
    if (millis() - last_time_bcast > 10000) {
      last_time_bcast = millis();
      // El epoch sigue siendo UTC. La zona viaja aparte porque son dos datos
      // distintos: el instante es medible, la hora local es una convencion que
      // alguien de fuera (NITZ o la consulta por IP) tiene que comunicarnos.
      //
      // tzsrc no es redundante con tzq: sin el, "offset 0 porque estamos en
      // Togo" y "offset 0 porque no lo sabemos" son indistinguibles, y el
      // display no puede decidir si pintar la hora o el aviso.
      char tmsg[48];
      snprintf(tmsg, sizeof(tmsg), "CTRL,TIME,%lu,%d,%d\n",
               (unsigned long)babyStore_nowEpoch(),
               (int)tz_source_quarters(), (int)tz_source_origin());
      hmiSerial.print(tmsg);
    }

    vTaskDelay(pdMS_TO_TICKS(COMMUNICATION_TASK_PERIOD_MS));
  }
}

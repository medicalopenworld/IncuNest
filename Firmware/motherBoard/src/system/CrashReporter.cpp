#include "CrashReporter.h"
#include "main.h"
#include "DriveUpload.h"
#include "board.h"

#include <LittleFS.h>
#include <esp_system.h>
#include "esp_log.h"

extern IncuNest_parameters in3;

// RTC slow memory. Survives abort/panic/SW reset; cleared on cold boot.
#define CRASH_RING_MAGIC 0xC0FFEE42u
RTC_NOINIT_ATTR static uint32_t s_ring_magic;
RTC_NOINIT_ATTR static uint32_t s_ring_head;
RTC_NOINIT_ATTR static uint32_t s_ring_full;
RTC_NOINIT_ATTR static uint32_t s_reboot_count;
RTC_NOINIT_ATTR static char     s_ring[CRASH_RING_SIZE];

// Captured at init time so we can flush later once FS and Drive are up.
static bool               s_pending_valid = false;
static esp_reset_reason_t s_pending_reason;
static uint32_t           s_pending_reboot_count;
static uint32_t           s_pending_head;
static uint32_t           s_pending_full;
static char              *s_pending_copy = nullptr;

static const char *resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
  case ESP_RST_POWERON:  return "POWERON";
  case ESP_RST_EXT:      return "EXT";
  case ESP_RST_SW:       return "SW";
  case ESP_RST_PANIC:    return "PANIC";
  case ESP_RST_INT_WDT:  return "INT_WDT";
  case ESP_RST_TASK_WDT: return "TASK_WDT";
  case ESP_RST_WDT:      return "WDT";
  case ESP_RST_DEEPSLEEP:return "DEEPSLEEP";
  case ESP_RST_BROWNOUT: return "BROWNOUT";
  case ESP_RST_SDIO:     return "SDIO";
  default:               return "UNKNOWN";
  }
}

static bool resetLooksLikeCrash(esp_reset_reason_t r) {
  return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
         r == ESP_RST_WDT   || r == ESP_RST_BROWNOUT;
}

// --------------------------------------------------------------------------
// Resumen publicable de la ultima caida (ver CrashReporter.h).
// --------------------------------------------------------------------------
static bool     s_summary_valid = false;
static char     s_summary_reason[16] = "";
static uint32_t s_summary_reboots = 0;
static char     s_summary_tail[CRASH_SUMMARY_TAIL_MAX] = "";

// Coge el FINAL del anillo --lo ultimo que se escribio antes de morir, que es
// lo que identifica la averia-- y lo deja en una sola linea apta para JSON.
static void buildSummaryTail(const char *ring, uint32_t head, uint32_t full) {
  if (ring == nullptr) {
    return;
  }
  // Reordena el anillo a lectura lineal: si dio la vuelta, lo ultimo esta
  // justo antes de head; si no, el contenido util va de 0 a head.
  const size_t used = full ? CRASH_RING_SIZE : (size_t)head;
  if (used == 0) {
    return;
  }
  const size_t want = (used < CRASH_SUMMARY_TAIL_MAX - 1)
                          ? used
                          : (size_t)(CRASH_SUMMARY_TAIL_MAX - 1);
  size_t out = 0;
  for (size_t i = used - want; i < used; i++) {
    // Indice real dentro del anillo.
    const size_t idx = full ? ((head + i) % CRASH_RING_SIZE) : i;
    char c = ring[idx];
    // Una sola linea y sin nada que rompa el JSON: los saltos de linea pasan a
    // " | " comprimido a un separador, y lo no imprimible se descarta.
    if (c == '\n' || c == '\r') {
      if (out > 0 && s_summary_tail[out - 1] != '|') {
        if (out + 2 >= CRASH_SUMMARY_TAIL_MAX) break;
        s_summary_tail[out++] = ' ';
        s_summary_tail[out++] = '|';
      }
      continue;
    }
    if (c == '"' || c == '\\' || (unsigned char)c < 0x20 ||
        (unsigned char)c > 0x7E) {
      continue;
    }
    if (out + 1 >= CRASH_SUMMARY_TAIL_MAX) break;
    s_summary_tail[out++] = c;
  }
  s_summary_tail[out] = '\0';
}

bool        crashReportPending(void) { return s_summary_valid; }
const char *crashReportReason(void) { return s_summary_reason; }
uint32_t    crashReportReboots(void) { return s_summary_reboots; }
const char *crashReportTail(void) { return s_summary_tail; }

void crashReporterInit() {
  esp_reset_reason_t reason = esp_reset_reason();

  bool ring_valid = (s_ring_magic == CRASH_RING_MAGIC);

  // Snapshot the ring before we overwrite it, so we can flush after FS is up.
  if (ring_valid && resetLooksLikeCrash(reason)) {
    s_pending_copy = (char *)malloc(CRASH_RING_SIZE);
    if (s_pending_copy) {
      memcpy(s_pending_copy, s_ring, CRASH_RING_SIZE);
      s_pending_head         = s_ring_head;
      s_pending_full         = s_ring_full;
      s_pending_reboot_count = s_reboot_count;
      s_pending_reason       = reason;
      s_pending_valid        = true;
    }
  }

  // Resumen para telemetria. Se arma aqui y no en el volcado a fichero porque
  // no puede depender de que LittleFS monte: una unidad que no consiga
  // escribir el informe tiene que poder contar igualmente por que se reinicio.
  if (s_pending_valid) {
    snprintf(s_summary_reason, sizeof(s_summary_reason), "%s",
             resetReasonStr(s_pending_reason));
    s_summary_reboots = s_pending_reboot_count;
    buildSummaryTail(s_pending_copy, s_pending_head, s_pending_full);
    s_summary_valid = true;
    // Sale siempre por consola: si el resumen llega vacio a ThingsBoard, esto
    // dice si el anillo estaba vacio o si el problema es el saneado.
    ESP_LOGW("CRASH", "resumen: reason=%s reboots=%u head=%u full=%u tail=[%s]",
             s_summary_reason, (unsigned)s_summary_reboots,
             (unsigned)s_pending_head, (unsigned)s_pending_full,
             s_summary_tail);
  }

  if (!ring_valid) {
    s_ring_magic    = CRASH_RING_MAGIC;
    s_reboot_count  = 0;
  }

  // Reset the ring for this boot. Reboot counter keeps growing until cold boot.
  s_ring_head    = 0;
  s_ring_full    = 0;
  s_reboot_count = s_reboot_count + 1;
}

void crashReporterPut(const char *data, size_t len) {
  if (s_ring_magic != CRASH_RING_MAGIC)
    return;
  for (size_t i = 0; i < len; i++) {
    s_ring[s_ring_head] = data[i];
    s_ring_head++;
    if (s_ring_head >= CRASH_RING_SIZE) {
      s_ring_head = 0;
      s_ring_full = 1;
    }
  }
}

void crashReporterMaybeFlush() {
  if (!s_pending_valid) {
    logDrive(String("no pending MB crash (reason=") +
             resetReasonStr(esp_reset_reason()) + ")");
    return;
  }

  logDrive(String("pending MB crash detected, reason=") +
           resetReasonStr(s_pending_reason) +
           " reboots=" + String(s_pending_reboot_count));

  char path[48];
  snprintf(path, sizeof(path), "/crash_mb_%lu.log",
           (unsigned long)millis());

  File f = LittleFS.open(path, "w", true);
  if (!f) {
    logDrive("cannot open crash file");
    free(s_pending_copy);
    s_pending_copy  = nullptr;
    s_pending_valid = false;
    return;
  }

  f.printf("=== IncuNest motherboard crash report ===\n");
  f.printf("FW      : %s\n", FWversion);
  f.printf("HW      : %d.%c\n", HW_NUM, HW_REVISION);
  f.printf("SN      : %d\n", (int)in3.serialNumber);
  f.printf("Reason  : %s (%d)\n", resetReasonStr(s_pending_reason),
           (int)s_pending_reason);
  f.printf("Reboots : %u (since last cold boot)\n",
           (unsigned)s_pending_reboot_count);
  f.printf("Uptime  : %lu ms (current boot)\n", (unsigned long)millis());
  f.printf("-- captured log (ring %u bytes, wrapped=%u) --\n",
           (unsigned)CRASH_RING_SIZE, (unsigned)s_pending_full);

  if (s_pending_full) {
    f.write((const uint8_t *)(s_pending_copy + s_pending_head),
            CRASH_RING_SIZE - s_pending_head);
    f.write((const uint8_t *)s_pending_copy, s_pending_head);
  } else {
    f.write((const uint8_t *)s_pending_copy, s_pending_head);
  }
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
    snprintf(drive_name, sizeof(drive_name), "%s_%d_crash_mb_%s.log", tsbuf,
             (int)in3.serialNumber, resetReasonStr(s_pending_reason));
  } else {
    snprintf(drive_name, sizeof(drive_name), "boot_%d_crash_mb_%s.log",
             (int)in3.serialNumber, resetReasonStr(s_pending_reason));
  }

  if (!driveEnqueueLogUpload(path, drive_name)) {
    logDrive("MB crash enqueue failed");
  } else {
    logDrive(String("queued MB crash: ") + drive_name);
  }

  free(s_pending_copy);
  s_pending_copy  = nullptr;
  s_pending_valid = false;
}

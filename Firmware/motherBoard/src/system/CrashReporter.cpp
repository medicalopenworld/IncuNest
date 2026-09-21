#include "CrashReporter.h"
#include "main.h"
#include "DriveUpload.h"
#include "board.h"

#include <LittleFS.h>
#include <stdarg.h>
#include <esp_system.h>
#include "esp_core_dump.h"
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
static char     s_summary_task[20] = "";
static char     s_summary_bt[96] = "";

// Resumen del coredump: la tarea que exploto y su backtrace.
//
// No se borra el coredump despues de leerlo: sigue en su particion para poder
// sacarlo entero con esptool y decodificarlo con addr2line. Solo se lee en el
// arranque siguiente a una caida, asi que no se republica en cada reinicio.
static void buildCoreDumpSummary(void) {
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
  esp_core_dump_summary_t *sum =
      (esp_core_dump_summary_t *)malloc(sizeof(esp_core_dump_summary_t));
  if (sum == NULL) {
    return;
  }
  const esp_err_t cd = esp_core_dump_get_summary(sum);
  if (cd != ESP_OK) {
    // Sale por consola para no quedarse adivinando por que va vacio: lo normal
    // es ESP_ERR_NOT_FOUND (la caida no llego a escribir coredump).
    ESP_LOGW("CRASH", "coredump no legible: %s", esp_err_to_name(cd));
    // Una imagen que no se puede leer no sirve para nada y ademas impide que
    // se escriba una buena: se borra para que la SIGUIENTE caida si deje un
    // volcado aprovechable. Solo se borra lo ilegible; un volcado valido se
    // conserva para sacarlo entero con esptool.
    if (cd == ESP_ERR_INVALID_SIZE || cd == ESP_ERR_INVALID_CRC ||
        cd == ESP_ERR_INVALID_VERSION) {
      const esp_err_t er = esp_core_dump_image_erase();
      ESP_LOGW("CRASH", "coredump ilegible borrado: %s", esp_err_to_name(er));
    }
  }
  if (cd == ESP_OK) {
    snprintf(s_summary_task, sizeof(s_summary_task), "%s", sum->exc_task);
    size_t out = 0;
    out += snprintf(s_summary_bt + out, sizeof(s_summary_bt) - out, "pc=0x%08x",
                    (unsigned)sum->exc_pc);
    for (uint32_t i = 0; i < sum->exc_bt_info.depth && out < sizeof(s_summary_bt) - 12; i++) {
      out += snprintf(s_summary_bt + out, sizeof(s_summary_bt) - out, " 0x%08x",
                      (unsigned)sum->exc_bt_info.bt[i]);
    }
  }
  free(sum);
#endif
}

// --------------------------------------------------------------------------
// Lineas que NO aportan nada en un informe de caida.
//
// La placa imprime la trama de estado al HMI y el keepalive del display una
// vez por segundo. Con una ventana corta, ese ruido periodico se come siempre
// lo interesante: el primer Crash_log que llego a ThingsBoard (unidad 1_2,
// 2026-09-21) eran exactamente esas dos lineas, que se habrian visto igual
// muriera de lo que muriera. Se descartan al componer el resumen; en el
// fichero completo de LittleFS siguen estando.
// --------------------------------------------------------------------------
static bool lineIsNoise(const char *line) {
  static const char *const kNoise[] = {
      "Sending state to HMI",
      "HMI CMD stored",
      "send_state_to_hmi",
      "Sending time to HMI",  // CTRL,TIME, tambien periodico
  };
  for (size_t i = 0; i < sizeof(kNoise) / sizeof(kNoise[0]); i++) {
    if (strstr(line, kNoise[i]) != NULL) {
      return true;
    }
  }
  return false;
}

// Recorre el anillo de ATRAS hacia delante quedandose con las ultimas lineas
// que dicen algo, y las devuelve en orden cronologico. Es la diferencia entre
// "lo ultimo que se imprimio" y "lo ultimo que importo".
static void buildSummaryTail(const char *ring, uint32_t head, uint32_t full) {
  if (ring == nullptr) {
    return;
  }
  const size_t used = full ? CRASH_RING_SIZE : (size_t)head;
  if (used == 0) {
    return;
  }
  // Copia lineal del anillo para poder partirlo en lineas sin pelearse con la
  // vuelta. Se libera antes de salir.
  char *lin = (char *)malloc(used + 1);
  if (lin == nullptr) {
    return;
  }
  for (size_t i = 0; i < used; i++) {
    lin[i] = ring[full ? ((head + i) % CRASH_RING_SIZE) : i];
  }
  lin[used] = '\0';

  // Primero TODOS los saltos de linea pasan a terminador, en una sola pasada
  // hacia delante. Hacerlo sobre la marcha durante el barrido hacia atras era
  // el fallo de la primera version: el retroceso buscaba el terminador
  // anterior, que todavia era un '\n' sin convertir, y se tragaba el anillo
  // entero como una unica linea gigante que no cabia en el presupuesto. El
  // resultado era un resumen vacio pese a tener el anillo lleno.
  for (size_t i = 0; i < used; i++) {
    if (lin[i] == '\n' || lin[i] == '\r') {
      lin[i] = '\0';
    }
  }

  // Ahora si: de la ultima linea a la primera.
  const size_t kMaxLines = 24;
  const char *starts[kMaxLines];
  size_t nlines = 0;
  size_t i = used;
  while (i > 0 && nlines < kMaxLines) {
    // saltar terminadores
    while (i > 0 && lin[i - 1] == '\0') {
      i--;
    }
    if (i == 0) {
      break;
    }
    const size_t end = i;
    while (i > 0 && lin[i - 1] != '\0') {
      i--;
    }
    // La primera linea del anillo suele estar cortada por la mitad (el anillo
    // dio la vuelta): se descarta para no publicar un trozo sin principio.
    if (i > 0 || used < CRASH_RING_SIZE) {
      (void)end;
      starts[nlines++] = &lin[i];
    }
  }

  // Quedarse con las utiles, de la mas reciente hacia atras, hasta llenar el
  // presupuesto; luego escribirlas en orden cronologico.
  const char *keep[kMaxLines];
  size_t nkeep = 0, budget = 0;
  for (size_t i = 0; i < nlines; i++) {
    const char *l = starts[i];
    if (l == NULL || l[0] == '\0' || lineIsNoise(l)) {
      continue;
    }
    const size_t len = strlen(l);
    if (budget + len + 3 >= CRASH_SUMMARY_TAIL_MAX) {
      break;
    }
    keep[nkeep++] = l;
    budget += len + 3;
  }

  size_t out = 0;
  for (size_t i = nkeep; i > 0; i--) {
    const char *l = keep[i - 1];
    for (const char *c = l; *c && out + 1 < CRASH_SUMMARY_TAIL_MAX; c++) {
      // Nada que rompa un JSON.
      if (*c == '"' || *c == '\\' || (unsigned char)*c < 0x20 ||
          (unsigned char)*c > 0x7E) {
        continue;
      }
      s_summary_tail[out++] = *c;
    }
    if (i > 1 && out + 3 < CRASH_SUMMARY_TAIL_MAX) {
      s_summary_tail[out++] = ' ';
      s_summary_tail[out++] = '|';
      s_summary_tail[out++] = ' ';
    }
  }
  s_summary_tail[out] = '\0';
  // Distinguir "no capture nada" de "no habia nada raro que capturar": si el
  // anillo tenia lineas pero todas eran trafico periodico, decirlo. Un campo
  // vacio no permite saber cual de las dos cosas paso.
  if (out == 0 && nlines > 0) {
    snprintf(s_summary_tail, CRASH_SUMMARY_TAIL_MAX,
             "(sin nada anomalo: %u lineas, todas trafico periodico)",
             (unsigned)nlines);
  }
  free(lin);
}

bool        crashReportPending(void) { return s_summary_valid; }
const char *crashReportReason(void) { return s_summary_reason; }
uint32_t    crashReportReboots(void) { return s_summary_reboots; }
const char *crashReportTail(void) { return s_summary_tail; }
const char *crashReportTask(void) { return s_summary_task; }
const char *crashReportBacktrace(void) { return s_summary_bt; }

// ---------------------------------------------------------------------------
// Puente del log de Arduino al anillo de caidas.
//
// build_src_flags define log_printf=incunest_log_printf SOLO para src/, asi
// que todos los ESP_LOGx/log_x de nuestro codigo entran aqui. Se copia la
// linea al anillo y se reenvia al log_printf de verdad del core, que es quien
// imprime. El framework y las librerias no pasan por aqui: siguen llamando al
// original.
//
// El #undef es imprescindible: sin el, la llamada de abajo se sustituiria por
// esta misma funcion y seria una recursion infinita.
// ---------------------------------------------------------------------------
#undef log_printf
extern "C" int log_printf(const char *fmt, ...);

extern "C" int incunest_log_printf(const char *fmt, ...) {
  char buf[256];
  va_list ring_args;
  va_start(ring_args, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ring_args);
  va_end(ring_args);
  if (n > 0) {
    crashReporterPut(buf, (size_t)(n < (int)sizeof(buf) ? n : (int)sizeof(buf)));
  }

  va_list out_args;
  va_start(out_args, fmt);
  // No se puede reenviar un va_list a una funcion variadica, asi que se imprime
  // aqui la cadena ya formateada. El formato y el orden son los mismos: buf lo
  // ha compuesto el mismo vsnprintf que usaria el core.
  int r = (n > 0) ? log_printf("%s", buf) : 0;
  va_end(out_args);
  return r;
}

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
    buildCoreDumpSummary();
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

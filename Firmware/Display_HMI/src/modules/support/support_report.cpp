#include "support_report.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "Credentials_public.h"
#include "Wifi_OTA.h"
#include "main.h"

// Diagnostico de arranque: definidos en main.cpp, sin header propio (mismo
// extern a mano que hace CommTask.cpp para la trama HMI,BOOT).
extern uint32_t g_hmiBootCount;
extern int g_hmiLastRst;

namespace {

// ---- Peticion pendiente -------------------------------------------------
// El mutex es un spinlock de FreeRTOS porque los dos lados (UI y tarea
// WiFi/OTA) corren en cores distintos y las copias son de unos cientos de
// bytes: mas barato que un semaforo y sin riesgo de inversion de prioridad
// bajo el mutex de LVGL.
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
volatile SupportRequestState s_state = SUPPORT_IDLE;
char s_subject[SUPPORT_SUBJECT_MAX];
char s_msg[SUPPORT_MESSAGE_MAX + 1];
char s_report[SUPPORT_REPORT_MAX];

// Escritor acotado: acumula printf() en un buffer y recuerda si se quedo sin
// sitio. Si desborda, el texto queda truncado en el ultimo campo completo y
// terminado en '\0'; nunca escribe fuera de `cap`.
struct Writer {
  char *out;
  size_t cap;
  size_t len = 0;
  bool overflow = false;

  Writer(char *o, size_t c) : out(o), cap(c) {
    if (cap) out[0] = '\0';
  }

  void add(const char *fmt, ...) {
    if (overflow || cap == 0) return;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(out + len, cap - len, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap - len) {
      overflow = true;
      // Deshacer el campo a medias: se prefiere un informe corto y correcto a
      // uno largo con el ultimo valor cortado.
      out[len] = '\0';
      return;
    }
    len += (size_t)n;
  }

  void addRaw(char c) {
    if (overflow || len + 1 >= cap) {
      overflow = true;
      return;
    }
    out[len++] = c;
    out[len] = '\0';
  }
};

const char *resetName(int rst) {
  switch (rst) {
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_EXT: return "ext";
    case ESP_RST_SW: return "sw";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}

const char *langCode() {
  return (g_lang == LANG_ES) ? "es" : (g_lang == LANG_FR) ? "fr" : "en";
}

const char *linkState() {
  if (!Display_BoardEverSeen()) return "never";
  return Display_IsBoardLinkLost() ? "lost" : "ok";
}

// Titulos de las alarmas activas separados por '|', acotados a `cap`. Si no
// caben todos, se corta con "..." en vez de dejar un titulo a medias.
void activeAlarmTitles(char *out, size_t cap) {
  size_t len = 0;
  out[0] = '\0';
  bool truncated = false;
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (!alarmList[i].state) continue;
    const char *title = alarmList[i].type;
    if (!title || !title[0]) continue;
    const size_t need = strlen(title) + (len ? 1 : 0);
    // Reserva 3 bytes para "..." si hubiera que cortar despues.
    if (len + need + 4 > cap) {
      truncated = true;
      break;
    }
    if (len) out[len++] = '|';
    memcpy(out + len, title, strlen(title));
    len += strlen(title);
    out[len] = '\0';
  }
  if (truncated && len + 4 <= cap) {
    memcpy(out + len, "...", 4);
  }
  if (len == 0 && !truncated) memcpy(out, "none", 5);
}

// RFC 3986 2.3: solo ALPHA / DIGIT / "-" / "." / "_" / "~" viajan tal cual.
bool isUnreserved(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
         c == '~';
}

void addEncoded(Writer &w, const char *s) {
  for (; *s && !w.overflow; s++) {
    const char c = *s;
    if (isUnreserved(c)) {
      w.addRaw(c);
    } else {
      w.add("%%%02X", (unsigned)(unsigned char)c);
    }
  }
}

}  // namespace

size_t support_report_build(char *out, size_t cap) {
  if (!out || cap == 0) return 0;
  Writer w(out, cap);

  // 1. Identidad y versiones: lo primero que soporte cruza con inventario.
  const char *mbFw = ctrl_state_msg.fwVer[0] ? ctrl_state_msg.fwVer : "?";
  const char hwRev = ctrl_state_msg.hwRev[0] ? ctrl_state_msg.hwRev[0] : '?';
  w.add("sn=%04d hmi=%s mb=%s hw=%d%c\n", in3.serialNumber, FWversion, mbFw,
        ctrl_state_msg.hwNum, hwRev);

  // 2. Arranque: cuantas veces y por que se ha reiniciado la pantalla.
  const uint32_t upS = millis() / 1000UL;
  w.add("boots=%u rst=%s up=%luh%02lum\n", (unsigned)g_hmiBootCount,
        resetName(g_hmiLastRst), (unsigned long)(upS / 3600UL),
        (unsigned long)((upS / 60UL) % 60UL));

  // 3. Red de la pantalla y servidor.
  const bool wifiUp = (WiFi.status() == WL_CONNECTED);
  if (wifiUp) {
    const IPAddress ip = WiFi.localIP();
    w.add("wifi=1 rssi=%d ip=%u.%u.%u.%u tb=%d\n", (int)WiFi.RSSI(),
          (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3],
          WIFIIsConnectedToServer() ? 1 : 0);
  } else {
    w.add("wifi=0 tb=0\n");
  }

  // 4. Enlace con la placa de control e idioma.
  w.add("link=%s bars=%d srv=%d lang=%s\n", linkState(), ctrl_state_msg.linkBars,
        ctrl_state_msg.serverCommStatus, langCode());

  // 5. Control: modo, actuacion y consignas.
  w.add("mode=%s act=%d setA=%.1f setS=%.1f setH=%d\n",
        (ctrl_state_msg.controlMode == CONTROL_AIR_MODE) ? "air" : "skin",
        ctrl_state_msg.actuation, (double)ctrl_state_msg.desiredAirTemperature,
        (double)ctrl_state_msg.desiredSkinTemperature,
        (int)ctrl_state_msg.desiredHumidity);

  // 6. Telemetria actual (la ultima recibida; si link=lost, esta congelada).
  w.add("air=%.1f skin=%.1f hum=%d probe=%d\n", (double)airTempValueDetected,
        (double)skinTempValueDetected, (int)humValueDetected,
        ctrl_state_msg.skinProbeState);

  // 7. Fototerapia y alarmas (bitmasks tal como los manda la placa).
  w.add("photo=%d alarms=0x%04lX sil=0x%04lX\n", ctrl_state_msg.phototherapyMode,
        (unsigned long)ctrl_state_msg.alarmBitmask,
        (unsigned long)ctrl_state_msg.silencedBitmask);

  // 8. Titulos de las alarmas activas: acotado para que siempre quede sitio
  // a la linea de memoria.
  char active[81];
  activeAlarmTitles(active, sizeof(active));
  w.add("active=%s\n", active);

  // 9. Memoria: libre interna / minimo historico / PSRAM.
  w.add("heap=%u/%u psram=%u",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  return w.len;
}

size_t support_report_subject(char *out, size_t cap) {
  if (!out || cap == 0) return 0;
  const int n = snprintf(out, cap, "IncuNest SN %04d - Solicitud de soporte",
                         in3.serialNumber);
  if (n < 0) {
    out[0] = '\0';
    return 0;
  }
  return ((size_t)n >= cap) ? cap - 1 : (size_t)n;
}

size_t support_report_build_mailto(char *out, size_t cap, const char *msg,
                                   bool withMessage, bool withReport) {
  if (!out || cap == 0) return 0;
  Writer w(out, cap);

  // La direccion no se codifica: '@' es valido en la parte de destinatario
  // de un mailto: y codificarlo confunde a algunos clientes de correo.
  w.add("mailto:%s?subject=", SUPPORT_EMAIL);

  char subject[SUPPORT_SUBJECT_MAX];
  support_report_subject(subject, sizeof(subject));
  addEncoded(w, subject);

  w.add("&body=");
  if (withMessage && msg && msg[0]) {
    addEncoded(w, msg);
    addEncoded(w, "\n\n");
  }
  if (withReport) {
    char report[SUPPORT_REPORT_MAX];
    support_report_build(report, sizeof(report));
    addEncoded(w, "-- IncuNest HMI --\n");
    addEncoded(w, report);
  }

  if (w.overflow) {
    out[0] = '\0';
    return 0;
  }
  return w.len;
}

// ---- Peticion pendiente ----------------------------------------------------

void SupportRequest_Submit(const char *msg) {
  // La instantanea se toma aqui, en el momento en que el operador pulsa
  // ENVIAR: es el estado que el quiere contar, no el de cuando la tarea WiFi
  // llegue a publicarlo.
  char subject[SUPPORT_SUBJECT_MAX];
  char report[SUPPORT_REPORT_MAX];
  support_report_subject(subject, sizeof(subject));
  support_report_build(report, sizeof(report));

  portENTER_CRITICAL(&s_mux);
  memcpy(s_subject, subject, sizeof(s_subject));
  memcpy(s_report, report, sizeof(s_report));
  if (msg) {
    strncpy(s_msg, msg, SUPPORT_MESSAGE_MAX);
    s_msg[SUPPORT_MESSAGE_MAX] = '\0';
  } else {
    s_msg[0] = '\0';
  }
  s_state = SUPPORT_PENDING;
  portEXIT_CRITICAL(&s_mux);
}

SupportRequestState SupportRequest_GetState(void) { return s_state; }

void SupportRequest_Reset(void) {
  portENTER_CRITICAL(&s_mux);
  s_state = SUPPORT_IDLE;
  portEXIT_CRITICAL(&s_mux);
}

bool SupportRequest_TakePending(char *subject, size_t subjectCap, char *msg,
                                size_t msgCap, char *report, size_t reportCap) {
  bool taken = false;
  portENTER_CRITICAL(&s_mux);
  if (s_state == SUPPORT_PENDING) {
    if (subject && subjectCap) {
      strncpy(subject, s_subject, subjectCap - 1);
      subject[subjectCap - 1] = '\0';
    }
    if (msg && msgCap) {
      strncpy(msg, s_msg, msgCap - 1);
      msg[msgCap - 1] = '\0';
    }
    if (report && reportCap) {
      strncpy(report, s_report, reportCap - 1);
      report[reportCap - 1] = '\0';
    }
    taken = true;
  }
  portEXIT_CRITICAL(&s_mux);
  return taken;
}

void SupportRequest_SetResult(bool ok) {
  portENTER_CRITICAL(&s_mux);
  // Solo si nadie ha reseteado entre medias: un resultado tardio no debe
  // resucitar una peticion que la UI ya dio por cerrada.
  if (s_state == SUPPORT_PENDING) s_state = ok ? SUPPORT_SENT : SUPPORT_FAILED;
  portEXIT_CRITICAL(&s_mux);
}

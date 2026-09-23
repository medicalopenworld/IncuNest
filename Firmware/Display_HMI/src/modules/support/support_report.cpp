#include "support_report.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
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

// Solo ASCII imprimible (mas '\n' como separador de lineas). Varios valores
// del informe vienen del protocolo serie (fwVer, hwRev, titulos de alarma),
// ASCII sin CRC: una linea corrupta puede colar un byte de control que
// acabaria dentro del mailto:.
char sanitize(char c) {
  if (c == '\n') return c;
  if (c < 0x20 || c > 0x7E) return '?';
  return c;
}

// Escritor acotado: acumula printf() en un buffer y recuerda si se quedo sin
// sitio. Si desborda, el texto queda truncado en el ultimo campo completo y
// terminado en '\0'; nunca escribe fuera de `cap`. Todo lo escrito pasa por
// sanitize().
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
    for (size_t i = len; i < len + (size_t)n; i++) out[i] = sanitize(out[i]);
    len += (size_t)n;
  }

  void addRaw(char c) {
    if (overflow || len + 1 >= cap) {
      overflow = true;
      return;
    }
    out[len++] = sanitize(c);
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

// Codigo del idioma en el informe de soporte. No es texto traducible: es el
// dato que le dice a soporte en que idioma esta viendo el equipo quien
// escribe. Un idioma que falte aqui se reporta como "en" y manda a soporte a
// mirar la pantalla equivocada, asi que va con switch y no con ternarios: al
// anadir un idioma a `ui_lang_t`, el -Wswitch avisa.
const char *langCode() {
  switch (g_lang) {
    case LANG_ES: return "es";
    case LANG_FR: return "fr";
    case LANG_PT: return "pt";
    case LANG_EN: return "en";
    case UI_LANG_COUNT: break;
  }
  return "en";
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
    // strnlen y no strlen: CommTask escribe alarmList[].type desde el otro
    // core sin lock, y entre su strncpy y el terminador hay una ventana en la
    // que el array puede no estar terminado.
    const size_t tlen = strnlen(title, ALARM_TYPE_LEN - 1);
    const size_t need = tlen + (len ? 1 : 0);
    // Reserva 3 bytes para "..." si hubiera que cortar despues.
    if (len + need + 4 > cap) {
      truncated = true;
      break;
    }
    if (len) out[len++] = '|';
    memcpy(out + len, title, tlen);
    len += tlen;
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

  // No deberia pasar con SUPPORT_REPORT_MAX (peor caso estimado ~380 B), pero
  // si pasa, que quede en el log: un informe sin la linea de memoria se
  // leeria como completo.
  if (w.overflow) {
    ESP_LOGW("Support", "informe truncado en %u B (cap %u)", (unsigned)w.len,
             (unsigned)cap);
  }
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

size_t support_report_build_mailto(char *out, size_t cap, bool withReport) {
  if (!out || cap == 0) return 0;
  Writer w(out, cap);

  // La direccion no se codifica: '@' es valido en la parte de destinatario
  // de un mailto: y codificarlo confunde a algunos clientes de correo.
  w.add("mailto:%s?subject=", SUPPORT_EMAIL);

  char subject[SUPPORT_SUBJECT_MAX];
  support_report_subject(subject, sizeof(subject));
  addEncoded(w, subject);

  if (withReport) {
    // Dos lineas en blanco arriba: ahi escribe el operador su consulta, y el
    // informe queda debajo como pie tecnico.
    w.add("&body=");
    addEncoded(w, "\n\n-- IncuNest HMI --\n");
    char report[SUPPORT_REPORT_MAX];
    support_report_build(report, sizeof(report));
    addEncoded(w, report);
  }

  if (w.overflow) {
    out[0] = '\0';
    return 0;
  }
  return w.len;
}

size_t mailto_build(char *out, size_t cap, const char *to, const char *subject,
                    const char *body) {
  if (!out || cap == 0 || !to) return 0;
  Writer w(out, cap);
  w.add("mailto:%s?subject=", to);
  if (subject) addEncoded(w, subject);
  if (body && body[0]) {
    w.add("&body=");
    addEncoded(w, body);
  }
  if (w.overflow) {
    out[0] = '\0';
    return 0;
  }
  return w.len;
}

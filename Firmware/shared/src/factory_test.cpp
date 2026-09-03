#include "factory_test.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// Claves ASCII de la tabla de motherBoard, en el orden de FtestId / design.md
// D10. Los arrays se declaran SIN tamano explicito para que el
// `static_assert` de abajo detecte tanto una clave de mas como una de menos
// si alguien toca el enum sin tocar aqui.
const char *const kMbKeys[] = {
    "sysinfo",  "ina3221",   "standby",  "charger",    "power_src",
    "skin_adc", "ext_sht4x", "sensor_src", "sb_link",  "sb_status",
    "sb_env",   "sb_door",   "sb_light", "sb_camera",  "actuators",
    "fan_rpm",  "humid_usb", "buzzer",   "afe_spi",    "afe_probe",
    "hmi_link", "gsm_at",    "gsm_sim",  "gsm_signal", "gsm_net",
    "wifi",     "tb_provision", "time",  "nvs",        "littlefs",
};
static_assert(sizeof(kMbKeys) / sizeof(kMbKeys[0]) ==
                  static_cast<unsigned>(FTEST_MB_COUNT),
              "kMbKeys debe tener exactamente FTEST_MB_COUNT entradas");

const char *const kHmiKeys[] = {
    "hmi_sysinfo", "hmi_i2c",  "hmi_panel", "hmi_touch", "hmi_buzzer",
    "hmi_speaker", "hmi_wifi", "hmi_nvs",   "link",
};
static_assert(sizeof(kHmiKeys) / sizeof(kHmiKeys[0]) ==
                  static_cast<unsigned>(FTEST_HMI_END - FTEST_HMI_BASE),
              "kHmiKeys debe cubrir FTEST_HMI_BASE..FTEST_HMI_END-1");

// Copia `detail` a `out` (buffer de `out_size` bytes, ya incluye el hueco
// para el terminador nulo) sustituyendo ',', '\r' y '\n' por ';' y truncando
// al tamano del buffer. `detail` NULL se trata como cadena vacia.
void sanitize_detail(const char *detail, char *out, size_t out_size) {
  size_t i = 0;
  if (detail != nullptr) {
    for (; detail[i] != '\0' && i + 1 < out_size; ++i) {
      char c = detail[i];
      if (c == ',' || c == '\r' || c == '\n') {
        c = ';';
      }
      out[i] = c;
    }
  }
  out[i] = '\0';
}

// Parsea un campo numerico sin signo que debe terminar exactamente en
// `delim`. Rechaza cadenas vacias, signos y cualquier caracter no numerico
// antes del delimitador (strtoul por si solo acepta espacios/signos que aqui
// no son validos).
bool parse_uint_field(const char *p, char delim, unsigned *out,
                       const char **after_delim) {
  if (p == nullptr || p[0] < '0' || p[0] > '9') {
    return false;
  }
  char *end = nullptr;
  const unsigned long v = strtoul(p, &end, 10);
  if (end == p || *end != delim) {
    return false;
  }
  *out = static_cast<unsigned>(v);
  *after_delim = end + 1;
  return true;
}

// Parsea el ULTIMO campo numerico de una linea: digitos seguidos, como mucho,
// de "\r" y/o "\n" y el fin de cadena. Cualquier otra cola se rechaza.
bool parse_uint_last(const char *p, unsigned *out) {
  if (p == nullptr || p[0] < '0' || p[0] > '9') {
    return false;
  }
  char *end = nullptr;
  const unsigned long v = strtoul(p, &end, 10);
  if (end == p) {
    return false;
  }
  while (*end == '\r' || *end == '\n') {
    ++end;
  }
  if (*end != '\0') {
    return false;
  }
  *out = static_cast<unsigned>(v);
  return true;
}

// true si, salvo un "\r"/"\n" final opcional, `p` es exactamente `keyword`.
bool matches_bare_keyword(const char *p, const char *keyword) {
  const size_t n = strlen(keyword);
  if (strncmp(p, keyword, n) != 0) {
    return false;
  }
  const char *rest = p + n;
  while (*rest == '\r' || *rest == '\n') {
    ++rest;
  }
  return *rest == '\0';
}

} // namespace

bool ftest_id_is_mb(unsigned id) { return id < static_cast<unsigned>(FTEST_MB_COUNT); }

bool ftest_id_is_hmi(unsigned id) {
  return id >= static_cast<unsigned>(FTEST_HMI_BASE) &&
         id < static_cast<unsigned>(FTEST_HMI_END);
}

bool ftest_id_is_optional(unsigned id) {
  switch (id) {
  case FTEST_MB_GSM_NET:
  case FTEST_MB_WIFI:
  case FTEST_MB_TB_PROVISION:
  case FTEST_MB_TIME:
  case FTEST_MB_AFE_PROBE:
    return true;
  default:
    return false;
  }
}

const char *ftest_id_key(unsigned id) {
  if (ftest_id_is_mb(id)) {
    return kMbKeys[id];
  }
  if (ftest_id_is_hmi(id)) {
    return kHmiKeys[id - static_cast<unsigned>(FTEST_HMI_BASE)];
  }
  return "?";
}

int ftest_format_result(char *buf, size_t n, unsigned id, FtestStatus st,
                         const char *detail) {
  if (buf == nullptr || n == 0 || !ftest_id_is_mb(id) || st > FTEST_CONFIRM) {
    return -1;
  }
  char safe[FTEST_DETAIL_MAX + 1];
  sanitize_detail(detail, safe, sizeof(safe));
  const int written = snprintf(buf, n, "CTRL,FTEST,%u,%u,%s\n", id,
                                static_cast<unsigned>(st), safe);
  if (written < 0 || static_cast<size_t>(written) >= n) {
    return -1;
  }
  return written;
}

int ftest_format_done(char *buf, size_t n, unsigned pass, unsigned fail,
                       unsigned skip) {
  if (buf == nullptr || n == 0) {
    return -1;
  }
  const int written =
      snprintf(buf, n, "CTRL,FTEST_DONE,%u,%u,%u\n", pass, fail, skip);
  if (written < 0 || static_cast<size_t>(written) >= n) {
    return -1;
  }
  return written;
}

int ftest_format_reject(char *buf, size_t n, FtestReject r) {
  if (buf == nullptr || n == 0 || r > FTEST_REJECT_UNKNOWN_ID) {
    return -1;
  }
  const int written =
      snprintf(buf, n, "CTRL,FTEST_REJECT,%u\n", static_cast<unsigned>(r));
  if (written < 0 || static_cast<size_t>(written) >= n) {
    return -1;
  }
  return written;
}

bool ftest_parse_result(const char *after_prefix, FtestResult *out) {
  if (after_prefix == nullptr || out == nullptr) {
    return false;
  }

  unsigned id = 0;
  const char *afterId = nullptr;
  if (!parse_uint_field(after_prefix, ',', &id, &afterId)) {
    return false;
  }

  unsigned status = 0;
  const char *afterStatus = nullptr;
  if (!parse_uint_field(afterId, ',', &status, &afterStatus)) {
    return false;
  }

  if (!ftest_id_is_mb(id) || status > static_cast<unsigned>(FTEST_CONFIRM)) {
    return false;
  }

  // detail = resto de la linea, sin el "\r\n" final si lo hay. Se trunca de
  // forma defensiva a FTEST_DETAIL_MAX: una linea bien formada nunca lo
  // supera (el codificador ya sanea), pero el parser no debe confiar en eso.
  size_t len = strlen(afterStatus);
  while (len > 0 &&
         (afterStatus[len - 1] == '\n' || afterStatus[len - 1] == '\r')) {
    --len;
  }
  if (len > FTEST_DETAIL_MAX) {
    len = FTEST_DETAIL_MAX;
  }
  memcpy(out->detail, afterStatus, len);
  out->detail[len] = '\0';
  out->id = static_cast<uint8_t>(id);
  out->status = static_cast<FtestStatus>(status);
  return true;
}

bool ftest_parse_done(const char *after_prefix, unsigned *pass, unsigned *fail,
                       unsigned *skip) {
  if (after_prefix == nullptr || pass == nullptr || fail == nullptr ||
      skip == nullptr) {
    return false;
  }

  unsigned p = 0, f = 0, s = 0;
  const char *afterPass = nullptr;
  if (!parse_uint_field(after_prefix, ',', &p, &afterPass)) {
    return false;
  }
  const char *afterFail = nullptr;
  if (!parse_uint_field(afterPass, ',', &f, &afterFail)) {
    return false;
  }
  if (!parse_uint_last(afterFail, &s)) {
    return false;
  }

  *pass = p;
  *fail = f;
  *skip = s;
  return true;
}

bool ftest_parse_reject(const char *after_prefix, FtestReject *out) {
  if (after_prefix == nullptr || out == nullptr) {
    return false;
  }
  unsigned v = 0;
  if (!parse_uint_last(after_prefix, &v) ||
      v > static_cast<unsigned>(FTEST_REJECT_UNKNOWN_ID)) {
    return false;
  }
  *out = static_cast<FtestReject>(v);
  return true;
}

bool ftest_parse_hmi_cmd(const char *after_prefix, FtestHmiCmd *out) {
  if (after_prefix == nullptr || out == nullptr) {
    return false;
  }

  if (matches_bare_keyword(after_prefix, "START")) {
    out->type = FTEST_CMD_START;
    out->id = FTEST_ID_NONE;
    out->ok = false;
    return true;
  }
  if (matches_bare_keyword(after_prefix, "ABORT")) {
    out->type = FTEST_CMD_ABORT;
    out->id = FTEST_ID_NONE;
    out->ok = false;
    return true;
  }
  if (strncmp(after_prefix, "RUN,", 4) == 0) {
    unsigned id = 0;
    if (!parse_uint_last(after_prefix + 4, &id) || !ftest_id_is_mb(id)) {
      return false;
    }
    out->type = FTEST_CMD_RUN;
    out->id = static_cast<uint8_t>(id);
    out->ok = false;
    return true;
  }
  if (strncmp(after_prefix, "CONFIRM,", 8) == 0) {
    unsigned id = 0;
    const char *afterId = nullptr;
    if (!parse_uint_field(after_prefix + 8, ',', &id, &afterId) ||
        id > 255u) {
      return false;
    }
    unsigned ok = 0;
    if (!parse_uint_last(afterId, &ok) || ok > 1u) {
      return false;
    }
    out->type = FTEST_CMD_CONFIRM;
    out->id = static_cast<uint8_t>(id);
    out->ok = (ok == 1u);
    return true;
  }

  return false;
}

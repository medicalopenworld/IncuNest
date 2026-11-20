#include "service_hours_proto.h"

#include <stdio.h>
#include <string.h>

namespace {

constexpr char kReportPrefix[] = "CTRL,SERVICE,";
constexpr char kDonePrefix[] = "HMI,SERVICE_DONE,";

// Campo numerico estricto: solo digitos, sin signo, 1..10 cifras, cabe en u32.
bool parseU32(const char *s, size_t len, uint32_t *out) {
  if (len == 0 || len > 10) return false;
  uint64_t v = 0;
  for (size_t i = 0; i < len; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = v * 10u + (uint64_t)(s[i] - '0');
  }
  if (v > 0xFFFFFFFFull) return false;
  *out = (uint32_t)v;
  return true;
}

// Parte `p` en campos separados por coma (punteros + longitudes, sin copiar).
// Devuelve el numero de campos, o -1 si hay mas de `maxFields`.
int splitFields(const char *p, const char **starts, size_t *lens,
                int maxFields) {
  int n = 0;
  for (;;) {
    if (n >= maxFields) return -1;
    const char *comma = strchr(p, ',');
    starts[n] = p;
    lens[n] = comma ? (size_t)(comma - p) : strlen(p);
    n++;
    if (!comma) return n;
    p = comma + 1;
  }
}

}  // namespace

int service_proto_build_report(char *buf, size_t len, const ServiceReport *r) {
  if (!buf || !r || len == 0 || r->n > SVC_COUNT) return 0;
  for (uint8_t i = 0; i < r->n; i++) {
    if (r->entry[i].id >= SVC_COUNT) return 0;
  }
  size_t pos = 0;
  int w = snprintf(buf, len, "CTRL,SERVICE,%u", (unsigned)r->n);
  if (w < 0 || (size_t)w >= len) return 0;
  pos = (size_t)w;
  for (uint8_t i = 0; i < r->n; i++) {
    w = snprintf(buf + pos, len - pos, ",%u,%lu,%lu", (unsigned)r->entry[i].id,
                 (unsigned long)r->entry[i].hours,
                 (unsigned long)r->entry[i].thresholdH);
    if (w < 0 || (size_t)w >= len - pos) return 0;
    pos += (size_t)w;
  }
  if (pos + 2 > len) return 0;
  buf[pos++] = '\n';
  buf[pos] = '\0';
  return (int)pos;
}

bool service_proto_parse_report(const char *line, ServiceReport *out) {
  if (!out) return false;
  out->n = 0;
  if (!line || strncmp(line, kReportPrefix, sizeof(kReportPrefix) - 1) != 0) {
    return false;
  }
  // n + 3 campos por entrada.
  const char *f[1 + 3 * SVC_COUNT];
  size_t fl[1 + 3 * SVC_COUNT];
  const int nf = splitFields(line + sizeof(kReportPrefix) - 1, f, fl,
                             1 + 3 * SVC_COUNT);
  uint32_t n = 0;
  if (nf < 1 || !parseU32(f[0], fl[0], &n) || n > SVC_COUNT) return false;
  if ((uint32_t)nf != 1 + 3 * n) return false;

  ServiceReport tmp;
  memset(&tmp, 0, sizeof(tmp));
  uint32_t seen = 0;  // bitmask de ids ya vistos
  for (uint32_t i = 0; i < n; i++) {
    uint32_t id = 0, hours = 0, thr = 0;
    if (!parseU32(f[1 + 3 * i], fl[1 + 3 * i], &id) ||
        !parseU32(f[2 + 3 * i], fl[2 + 3 * i], &hours) ||
        !parseU32(f[3 + 3 * i], fl[3 + 3 * i], &thr)) {
      return false;
    }
    if (id >= SVC_COUNT || (seen & (1u << id))) return false;
    seen |= 1u << id;
    tmp.entry[i].id = (uint8_t)id;
    tmp.entry[i].hours = hours;
    tmp.entry[i].thresholdH = thr;
  }
  tmp.n = (uint8_t)n;
  *out = tmp;
  return true;
}

int service_proto_build_done(char *buf, size_t len, uint8_t id) {
  if (!buf || len == 0 || id >= SVC_COUNT) return 0;
  const int w = snprintf(buf, len, "HMI,SERVICE_DONE,%u\n", (unsigned)id);
  if (w < 0 || (size_t)w >= len) return 0;
  return w;
}

bool service_proto_parse_done(const char *line, uint8_t *id) {
  if (!line || !id) return false;
  if (strncmp(line, kDonePrefix, sizeof(kDonePrefix) - 1) != 0) return false;
  const char *f = line + sizeof(kDonePrefix) - 1;
  uint32_t v = 0;
  if (!parseU32(f, strlen(f), &v) || v >= SVC_COUNT) return false;
  *id = (uint8_t)v;
  return true;
}

bool service_proto_is_req(const char *line) {
  return line && strcmp(line, "HMI,SERVICE_REQ") == 0;
}

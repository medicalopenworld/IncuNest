#include "family_proto.h"

#include <stdio.h>
#include <string.h>

namespace {
constexpr char kPrefix[] = "CTRL,FAMILY,";
constexpr char kScheme[] = "https://";
constexpr size_t kSchemeLen = sizeof(kScheme) - 1;
}  // namespace

bool family_url_valid(const char *url) {
  if (!url) return false;
  const size_t n = strnlen(url, FAMILY_URL_MAX + 1);
  if (n <= kSchemeLen || n > FAMILY_URL_MAX) return false;
  if (strncmp(url, kScheme, kSchemeLen) != 0) return false;
  for (size_t i = 0; i < n; i++) {
    const unsigned char c = (unsigned char)url[i];
    if (c < 0x21 || c > 0x7E || c == ',') return false;
  }
  return true;
}

int family_proto_build(char *buf, size_t len, const FamilyMsg *m) {
  if (!buf || !m || len == 0) return 0;
  if (m->url[0] != '\0' && !family_url_valid(m->url)) return 0;
  const int w = snprintf(buf, len, "CTRL,FAMILY,%d,%s\n", m->notice ? 1 : 0,
                         m->url);
  if (w < 0 || (size_t)w >= len) return 0;
  return w;
}

bool family_proto_parse(const char *line, FamilyMsg *out) {
  if (!out) return false;
  out->notice = false;
  out->url[0] = '\0';
  if (!line || strncmp(line, kPrefix, sizeof(kPrefix) - 1) != 0) return false;
  const char *p = line + sizeof(kPrefix) - 1;
  if ((p[0] != '0' && p[0] != '1') || p[1] != ',') return false;
  const bool notice = p[0] == '1';
  const char *url = p + 2;
  if (strchr(url, ',')) return false;  // campo de mas
  if (url[0] != '\0' && !family_url_valid(url)) return false;
  out->notice = notice;
  strncpy(out->url, url, FAMILY_URL_MAX);
  out->url[FAMILY_URL_MAX] = '\0';
  return true;
}

#include "sb_env_report.h"

#include <string.h>

// ESQUELETO DE LA FASE ROJA: compila y no hace nada util. Las pruebas de
// test/test_sensorboard_env_report fallan contra esto; la implementacion
// llega en el commit siguiente.

void sb_env_report_make(SbEnvReport *out, const SbSnapshot *s, uint32_t now) {
  (void)s;
  (void)now;
  memset(out, 0, sizeof(*out));
}

int sb_env_report_format(char *buf, size_t size, const SbEnvReport *r) {
  (void)r;
  if (size > 0) buf[0] = '\0';
  return 0;
}

void sb_env_pacer_init(SbEnvPacer *p) { memset(p, 0, sizeof(*p)); }

bool sb_env_pacer_due(const SbEnvPacer *p, const SbEnvReport *r,
                      uint32_t now) {
  (void)p;
  (void)r;
  (void)now;
  return false;
}

void sb_env_pacer_sent(SbEnvPacer *p, const SbEnvReport *r, uint32_t now) {
  (void)p;
  (void)r;
  (void)now;
}

#include "sb_env.h"

// ESQUELETO DE LA FASE ROJA: compila y no hace nada util. Las pruebas de
// test/test_sb_env fallan contra esto; la implementacion llega en el commit
// siguiente.

bool sbenv_parse(const char *line, SbEnvMsg *out) {
  (void)line;
  (void)out;
  return false;
}

SbEnvNoise sbenv_noise_level(int dba) {
  (void)dba;
  return SBENV_NOISE_NONE;
}

SbEnvView sbenv_view(bool have, const SbEnvMsg *msg, uint32_t rx_ms,
                     uint32_t now_ms, bool boardLinkLost) {
  (void)have;
  (void)msg;
  (void)rx_ms;
  (void)now_ms;
  (void)boardLinkLost;
  SbEnvView v = {false, SBENV_NOISE_NONE};
  return v;
}

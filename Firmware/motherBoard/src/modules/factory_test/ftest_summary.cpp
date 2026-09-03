#include "ftest_summary.h"

namespace {

// Los ids de motherBoard caben en 0..FTEST_MB_COUNT-1 (<= 32, D1), asi que un
// bit por id cabe siempre en un uint32_t.
uint32_t bit_of(unsigned id) { return 1u << id; }

} // namespace

void ftest_summary_init(FtestSummary *s) {
  if (s == nullptr) {
    return;
  }
  s->pass_mask = 0u;
  s->fail_mask = 0u;
  s->run_mask = 0u;
  s->pass = 0u;
  s->fail = 0u;
  s->skip = 0u;
}

void ftest_summary_note(FtestSummary *s, unsigned id, FtestStatus st) {
  if (s == nullptr || !ftest_id_is_mb(id)) {
    return;
  }
  if (st == FTEST_RUNNING || st == FTEST_WAIT || st == FTEST_CONFIRM) {
    return; // no son resultados finales
  }

  const uint32_t bit = bit_of(id);
  if (s->run_mask & bit) {
    return; // ya contabilizado: la primera notificacion final gana
  }
  s->run_mask |= bit;

  switch (st) {
  case FTEST_PASS:
    s->pass_mask |= bit;
    s->pass++;
    break;
  case FTEST_FAIL:
    s->fail_mask |= bit;
    s->fail++;
    break;
  case FTEST_SKIP:
    s->skip++;
    break;
  default:
    break; // inalcanzable: los estados transitorios ya se filtraron arriba
  }
}

void ftest_summary_merge_single(uint32_t *pass_mask, uint32_t *fail_mask,
                                 uint32_t *run_mask, unsigned id,
                                 FtestStatus st) {
  if (pass_mask == nullptr || fail_mask == nullptr || run_mask == nullptr ||
      !ftest_id_is_mb(id)) {
    return;
  }

  const uint32_t bit = bit_of(id);
  // Reintento: el resultado anterior de este id, sea cual sea, se olvida.
  *pass_mask &= ~bit;
  *fail_mask &= ~bit;
  *run_mask |= bit;

  if (st == FTEST_PASS) {
    *pass_mask |= bit;
  } else if (st == FTEST_FAIL) {
    *fail_mask |= bit;
  }
  // SKIP (u otro estado): el bit queda solo en run_mask.
}

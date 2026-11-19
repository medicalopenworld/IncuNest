#include "modules/trend/trend_decimate.h"

namespace {

// Division entera con redondeo hacia -inf (b > 0). Las ranuras son negativas
// en el arranque sin hora: now - n + 1 puede ser < 0.
int64_t floorDiv(int64_t a, int64_t b) {
  int64_t q = a / b;
  if ((a % b) < 0) q--;
  return q;
}

}  // namespace

int trend_window_slots(int idx) {
  switch (idx) {
    case 1:
      return 4320;  // 12 h
    case 2:
      return 8640;  // 24 h
    default:
      return 1440;  // 4 h
  }
}

TrendWindow trend_window_plan(int64_t nowSlot, int idx) {
  const int n = trend_window_slots(idx);
  TrendWindow w;
  w.slotsPerBucket = n / TREND_BASE_BUCKETS;  // 2 / 6 / 12
  w.buckets = TREND_BUCKETS;
  w.totalSlots = (int64_t)w.buckets * w.slotsPerBucket;
  // Limites alineados a multiplos absolutos del tamano de cubo. Con start >=
  // (now-n+1) - (spb-1) el eje cubre al menos las n ranuras pedidas y, como
  // tiene 720*spb + spb ranuras, siempre contiene "ahora".
  w.startSlot =
      floorDiv(nowSlot - n + 1, w.slotsPerBucket) * w.slotsPerBucket;
  return w;
}

void trend_decimate(const TrendStore *s, int series, const TrendWindow *w,
                    int16_t *out) {
  if (!w || !out) return;
  for (int b = 0; b < w->buckets; b++) {
    const int64_t s0 = w->startSlot + (int64_t)b * w->slotsPerBucket;
    int16_t mn = 0, mx = 0;
    int mnIdx = -1, mxIdx = -1;  // primera aparicion de cada extremo
    for (int k = 0; k < w->slotsPerBucket; k++) {
      const int16_t v = trend_store_value(s, series, s0 + k);
      if (v == TREND_NO_DATA) continue;
      if (mnIdx < 0 || v < mn) {
        mn = v;
        mnIdx = k;
      }
      if (mxIdx < 0 || v > mx) {
        mx = v;
        mxIdx = k;
      }
    }
    if (mnIdx < 0) {
      out[2 * b] = TREND_NO_DATA;
      out[2 * b + 1] = TREND_NO_DATA;
    } else if (mnIdx <= mxIdx) {
      out[2 * b] = mn;  // el minimo ocurrio primero (o son el mismo punto)
      out[2 * b + 1] = mx;
    } else {
      out[2 * b] = mx;
      out[2 * b + 1] = mn;
    }
  }
}

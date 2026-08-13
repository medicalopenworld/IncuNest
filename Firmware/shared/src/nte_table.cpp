#include "nte_table.h"

// Neutral Thermal Environment table.
// Clinical source: "Termorregulacion y Humedad en el RN" — Sauer/Dane/Visser
// (1984); Deacon/O'Neill (2004); Cloherty 3rd ed.; WHO/AAP.
// Ported from the former Display_HMI/src/tasks/UITask.cpp:901-1129
// (AUTO AIR popup) as the single shared implementation — see
// specs/nte-calculation-engine/spec.md, "Single shared implementation".

namespace {

NteRange invalidRange() { return {-1.0f, -1.0f, -1.0f, true}; }

// Time-period index: 0=D1, 1=D2, 2=D3, 3=D4-7, 4=W2, 5=W3, 6=W4+
int timePeriodIndex(uint32_t ageHours) {
  if (ageHours < 24) return 0;
  if (ageHours < 48) return 1;
  if (ageHours < 72) return 2;
  if (ageHours < 168) return 3;
  if (ageHours < 336) return 4;
  if (ageHours < 504) return 5;
  return 6;
}

} // namespace

NteRange calculateNteRange(uint16_t weightGrams, uint8_t gestWeeks,
                            uint16_t ageDays) {
  if (weightGrams == 0 || gestWeeks < 24) {
    return invalidRange();
  }

  const uint32_t ageHours = (uint32_t)ageDays * 24u;
  const int tp = timePeriodIndex(ageHours);

  float lo, hi;

  if (gestWeeks <= 27) {
    // EG 24-27 semanas (extremadamente prematuro): 3 weight rows x 7 time
    // periods x [lo, hi]
    static const float T[3][7][2] = {
        // <750 g
        {{35.0f, 36.0f},
         {34.5f, 35.5f},
         {34.0f, 35.0f},
         {33.5f, 34.5f},
         {33.0f, 34.0f},
         {32.5f, 33.5f},
         {32.0f, 33.0f}},
        // 750-1000 g
        {{34.5f, 35.5f},
         {34.0f, 35.0f},
         {34.0f, 35.0f},
         {33.5f, 34.5f},
         {33.0f, 34.0f},
         {32.5f, 33.5f},
         {32.0f, 33.0f}},
        // 1000-1200 g (also used conservatively if weight > 1200 at this EG)
        {{34.0f, 35.4f},
         {34.0f, 35.0f},
         {34.0f, 35.0f},
         {33.0f, 34.0f},
         {32.6f, 34.0f},
         {32.2f, 34.0f},
         {31.6f, 33.6f}},
    };
    int wr = (weightGrams < 750) ? 0 : (weightGrams <= 1000) ? 1 : 2;
    lo = T[wr][tp][0];
    hi = T[wr][tp][1];

  } else if (gestWeeks <= 31) {
    // EG 28-31 semanas (muy prematuro)
    static const float T[3][7][2] = {
        // <1200 g
        {{34.0f, 35.4f},
         {34.0f, 35.0f},
         {34.0f, 35.0f},
         {33.0f, 34.0f},
         {32.6f, 34.0f},
         {32.2f, 34.0f},
         {31.6f, 33.6f}},
        // 1200-1500 g
        {{33.9f, 34.4f},
         {33.1f, 34.2f},
         {33.0f, 34.0f},
         {33.0f, 34.0f},
         {31.0f, 33.2f},
         {30.5f, 33.0f},
         {30.0f, 32.7f}},
        // 1501-1800 g (also used conservatively if weight > 1800 at this EG)
        {{33.5f, 34.5f},
         {33.0f, 34.0f},
         {32.5f, 33.5f},
         {32.0f, 33.5f},
         {31.0f, 33.2f},
         {30.5f, 33.0f},
         {30.0f, 32.7f}},
    };
    int wr = (weightGrams < 1200) ? 0 : (weightGrams <= 1500) ? 1 : 2;
    lo = T[wr][tp][0];
    hi = T[wr][tp][1];

  } else if (gestWeeks <= 35) {
    // EG 32-35 semanas (prematuro moderado/tardio); >2500g W3 and W4+ are "-"
    static const float T[3][7][2] = {
        // 1200-1500 g (also used if weight < 1200 at this EG - conservative)
        {{33.9f, 34.4f},
         {33.0f, 34.1f},
         {33.0f, 34.0f},
         {33.0f, 34.0f},
         {31.0f, 33.2f},
         {30.5f, 33.0f},
         {30.0f, 32.7f}},
        // 1501-2500 g
        {{32.8f, 33.8f},
         {31.6f, 33.6f},
         {31.2f, 33.4f},
         {31.1f, 33.2f},
         {31.0f, 33.2f},
         {30.5f, 33.0f},
         {30.0f, 32.7f}},
        // >2500 g (W3=tp5 and W4+=tp6 are "-")
        {{32.0f, 33.8f},
         {30.7f, 33.5f},
         {30.1f, 33.2f},
         {29.8f, 32.8f},
         {29.0f, 31.4f},
         {-1.0f, -1.0f},
         {-1.0f, -1.0f}},
    };
    int wr = (weightGrams <= 1500) ? 0 : (weightGrams <= 2500) ? 1 : 2;
    lo = T[wr][tp][0];
    hi = T[wr][tp][1];

  } else {
    // EG >= 36 semanas (cercano a termino / termino); all rows: W3+ (tp>=5)
    // are "-"
    static const float T[3][5][2] = {
        // 1501-2500 g (also used conservatively if weight <= 1500 at this EG)
        {{32.8f, 33.8f},
         {31.4f, 33.5f},
         {31.2f, 33.4f},
         {31.0f, 33.2f},
         {29.0f, 31.4f}},
        // 2500-3500 g
        {{32.0f, 33.8f},
         {30.5f, 33.3f},
         {30.1f, 33.2f},
         {29.5f, 32.6f},
         {29.0f, 30.8f}},
        // >3500 g
        {{31.5f, 33.5f},
         {30.0f, 33.0f},
         {29.8f, 32.8f},
         {29.5f, 32.0f},
         {29.0f, 30.5f}},
    };
    if (tp >= 5) {
      return invalidRange();
    }
    int wr = (weightGrams <= 2500) ? 0 : (weightGrams <= 3500) ? 1 : 2;
    lo = T[wr][tp][0];
    hi = T[wr][tp][1];
  }

  // "-" cells (lo stored as -1 sentinel)
  if (lo < 0.0f || hi < 0.0f) {
    return invalidRange();
  }

  return {lo, hi, (lo + hi) / 2.0f, false};
}

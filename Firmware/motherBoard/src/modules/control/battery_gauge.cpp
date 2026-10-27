#include "modules/control/battery_gauge.h"

// Tension en reposo de una bateria de plomo de 12 V frente a su carga, a
// 25 C (tabla habitual de fabricantes de AGM/SLA). El 0 % es el corte bajo
// carga de 10,5 V: por debajo de 11,5 V la curva cae rapido y el indicador ya
// esta en la ultima raya.
typedef struct {
  uint16_t mv;
  uint8_t pct;
} CurvePoint;

static const CurvePoint kCurve[] = {
    {10500, 0},  {11510, 10}, {11660, 20}, {11810, 30},
    {11960, 40}, {12100, 50}, {12240, 60}, {12370, 70},
    {12500, 80}, {12620, 90}, {12730, 100},
};
static const unsigned kCurveLen = sizeof(kCurve) / sizeof(kCurve[0]);

uint8_t battery_gauge_percent_from_mv(uint32_t mv) {
  if (mv <= kCurve[0].mv) return kCurve[0].pct;
  if (mv >= kCurve[kCurveLen - 1].mv) return kCurve[kCurveLen - 1].pct;
  for (unsigned i = 1; i < kCurveLen; i++) {
    if (mv <= kCurve[i].mv) {
      const uint32_t dmv = kCurve[i].mv - kCurve[i - 1].mv;
      const uint32_t dpct = kCurve[i].pct - kCurve[i - 1].pct;
      return (uint8_t)(kCurve[i - 1].pct +
                       (mv - kCurve[i - 1].mv) * dpct / dmv);
    }
  }
  return kCurve[kCurveLen - 1].pct;
}

uint8_t battery_gauge_level_from_percent(uint8_t percent) {
  if (percent >= BATT_GAUGE_PCT_4_BARS) return 4;
  if (percent >= BATT_GAUGE_PCT_3_BARS) return 3;
  if (percent >= BATT_GAUGE_PCT_2_BARS) return 2;
  if (percent >= BATT_GAUGE_PCT_1_BAR) return 1;
  return 0;
}

void battery_gauge_sample(BatteryGauge *g, bool valid, bool ac_present,
                          bool full, uint16_t vbat_mv, uint16_t idchg_ma) {
  if (g == 0 || !valid) return;

  if (ac_present) {
    // Con red la tension la fija el cargador: no se estima nada. Lleno es
    // lleno; cargando conserva el ultimo nivel conocido, que el display no
    // pinta como medida (anima la carga).
    g->filtered = false;
    if (full) {
      g->state = BATT_GAUGE_FULL;
      g->level = BATT_GAUGE_LEVEL_MAX;
      g->percent = 100;
    } else {
      g->state = BATT_GAUGE_CHARGING;
    }
    return;
  }

  const uint32_t vcomp_mv =
      (uint32_t)vbat_mv + (uint32_t)idchg_ma * BATT_GAUGE_RINT_MOHM / 1000u;
  const uint32_t x16 = vcomp_mv * 16u;
  const bool entering = (g->state != BATT_GAUGE_DISCHARGING) || !g->filtered;
  if (entering) {
    // Primera muestra de la descarga: siembra la media (si arrancara en 0 V la
    // primera lectura diria "vacia") y libera el tope de rayas de la anterior.
    g->vcomp_mv_x16 = x16;
    g->filtered = true;
  } else {
    const int32_t diff = (int32_t)x16 - (int32_t)g->vcomp_mv_x16;
    g->vcomp_mv_x16 = (uint32_t)((int32_t)g->vcomp_mv_x16 +
                                 diff / BATT_GAUGE_EMA_DIV);
  }

  const uint8_t pct = battery_gauge_percent_from_mv(g->vcomp_mv_x16 / 16u);
  const uint8_t level = battery_gauge_level_from_percent(pct);
  if (entering) {
    g->percent = pct;
    g->level = level;
  } else {
    // Solo bajan durante la descarga.
    if (pct < g->percent) g->percent = pct;
    if (level < g->level) g->level = level;
  }
  g->state = BATT_GAUGE_DISCHARGING;
}

bool battery_gauge_report_low(uint32_t report) {
  const uint32_t state = (report >> 16) & 0xFFu;
  const uint32_t percent = report & 0xFFu;
  return state == BATT_GAUGE_DISCHARGING && percent <= BATT_GAUGE_LOW_PCT;
}

uint32_t battery_gauge_pack(const BatteryGauge *g) {
  if (g == 0) return 0;
  return ((uint32_t)g->state << 16) | ((uint32_t)g->level << 8) |
         (uint32_t)g->percent;
}

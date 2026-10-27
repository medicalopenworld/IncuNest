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

static const uint32_t kUpctPerPct = 1000000u;

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

void battery_gauge_restore(BatteryGauge *g, uint8_t percent) {
  if (g == 0) return;
  g->percent = (percent > 100u) ? 100u : percent;
  g->level = battery_gauge_level_from_percent(g->percent);
  g->known = true;
}

static void set_percent(BatteryGauge *g, uint8_t pct) {
  g->percent = pct;
  g->level = battery_gauge_level_from_percent(pct);
}

static void sample_mains(BatteryGauge *g, bool full, uint16_t ichg_ma,
                         uint32_t dt_ms) {
  g->filtered = false;
  if (full) {
    g->state = BATT_GAUGE_FULL;
    g->charge_upct = 100u * kUpctPerPct;
    g->known = true;
    set_percent(g, 100);
    return;
  }

  const uint32_t cap_upct = BATT_GAUGE_CHARGING_MAX_PCT * kUpctPerPct;
  if (g->state != BATT_GAUGE_CHARGING) {
    // Se empieza a cargar desde lo ultimo conocido (0 sin historia: el lado
    // seguro, se ve menos carga de la que hay).
    const uint32_t start = g->known ? g->percent : 0u;
    g->charge_upct = start * kUpctPerPct;
    if (g->charge_upct > cap_upct) g->charge_upct = cap_upct;
    g->state = BATT_GAUGE_CHARGING;
  }

  // mAh = mA * ms / 3.6e6; puntos = mAh / capacidad * rendimiento.
  const uint64_t num = (uint64_t)ichg_ma * dt_ms * BATT_GAUGE_CHARGE_EFF_PCT *
                       (uint64_t)kUpctPerPct;
  const uint64_t den = 3600000ull * BATT_GAUGE_CAPACITY_MAH;
  uint64_t upct = (uint64_t)g->charge_upct + num / den;
  if (upct > cap_upct) upct = cap_upct;
  g->charge_upct = (uint32_t)upct;
  g->known = true;
  set_percent(g, (uint8_t)(g->charge_upct / kUpctPerPct));
}

static void sample_battery(BatteryGauge *g, uint16_t vbat_mv,
                           uint16_t idchg_ma) {
  const uint32_t vcomp_mv =
      (uint32_t)vbat_mv + (uint32_t)idchg_ma * BATT_GAUGE_RINT_MOHM / 1000u;
  const uint32_t x16 = vcomp_mv * 16u;
  const bool entering = (g->state != BATT_GAUGE_DISCHARGING) || !g->filtered;
  if (entering) {
    // Primera muestra de la descarga: siembra la media (si arrancara en 0 V la
    // primera lectura diria "vacia").
    g->vcomp_mv_x16 = x16;
    g->filtered = true;
  } else {
    const int32_t diff = (int32_t)x16 - (int32_t)g->vcomp_mv_x16;
    g->vcomp_mv_x16 = (uint32_t)((int32_t)g->vcomp_mv_x16 +
                                 diff / BATT_GAUGE_EMA_DIV);
  }

  uint8_t pct = battery_gauge_percent_from_mv(g->vcomp_mv_x16 / 16u);
  if (entering) {
    // Recien quitada la red la tension aun lleva la carga superficial de la
    // flotacion: manda lo que se sabia si es menor.
    if (g->known && g->percent < pct) pct = g->percent;
    set_percent(g, pct);
  } else if (pct < g->percent) {
    // Solo baja durante la descarga.
    set_percent(g, pct);
  }
  g->known = true;
  g->state = BATT_GAUGE_DISCHARGING;
}

void battery_gauge_sample(BatteryGauge *g, bool valid, bool ac_present,
                          bool full, uint16_t vbat_mv, uint16_t idchg_ma,
                          uint16_t ichg_ma, uint32_t dt_ms) {
  if (g == 0 || !valid) return;
  if (ac_present) {
    sample_mains(g, full, ichg_ma, dt_ms);
  } else {
    sample_battery(g, vbat_mv, idchg_ma);
  }
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

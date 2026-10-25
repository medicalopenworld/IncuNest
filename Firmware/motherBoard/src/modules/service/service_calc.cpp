#include "service_calc.h"

#include <math.h>

#include "config/service_thresholds.h"

float service_hours_since(float lifeHours, float baseHours) {
  if (!(lifeHours >= 0.0f)) return 0.0f;  // negativo o NaN
  if (!(baseHours >= 0.0f)) baseHours = 0.0f;
  if (lifeHours < baseHours) return lifeHours;
  return lifeHours - baseHours;
}

uint32_t service_whole_hours(float hours) {
  if (!(hours >= 1.0f)) return 0;
  // 4294967296.0f es 2^32 exacto en float: por encima no cabe en uint32.
  if (hours >= 4294967296.0f) return 0xFFFFFFFFu;
  return (uint32_t)hours;
}

bool service_is_due(uint32_t sinceHours, uint32_t thresholdHours) {
  return thresholdHours != 0 && sinceHours >= thresholdHours;
}

uint32_t service_threshold_hours(uint8_t id) {
  switch (id) {
    case SVC_HEATER:
      return SERVICE_THRESHOLD_HEATER_H;
    case SVC_FAN:
      return SERVICE_THRESHOLD_FAN_H;
    case SVC_HUMIDIFIER:
      return SERVICE_THRESHOLD_HUMIDIFIER_H;
    case SVC_PHOTO:
      return SERVICE_THRESHOLD_PHOTO_H;
    default:
      return 0;
  }
}

void service_calc_report(const float *life, const float *base,
                         const uint32_t *thr, ServiceReport *out) {
  if (!out) return;
  out->n = 0;
  if (!life || !base || !thr) return;
  for (uint8_t i = 0; i < SVC_COUNT; i++) {
    out->entry[i].id = i;
    out->entry[i].hours = service_whole_hours(service_hours_since(life[i], base[i]));
    out->entry[i].thresholdH = thr[i];
  }
  out->n = SVC_COUNT;
}

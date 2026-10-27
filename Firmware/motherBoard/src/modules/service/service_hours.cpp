#include "service_hours.h"

#include <Preferences.h>

#include "config/preferences_keys.h"
#include "esp_log.h"
#include "main.h"  // in3
#include "service_calc.h"

extern IncuNest_parameters in3;

namespace {

constexpr const char *TAG = "SERVICE";

const char *const KEY_BASE[SVC_COUNT] = {
    KEY_SVC_BASE_HEATER, KEY_SVC_BASE_FAN, KEY_SVC_BASE_HUM,
    KEY_SVC_BASE_PHOTO};
const char *const KEY_EPOCH[SVC_COUNT] = {
    KEY_SVC_EPOCH_HEATER, KEY_SVC_EPOCH_FAN, KEY_SVC_EPOCH_HUM,
    KEY_SVC_EPOCH_PHOTO};

// Horas de vida de cada actuador, en el orden de ServiceActuator.
float lifeHours(uint8_t id) {
  switch (id) {
    case SVC_HEATER:
      return in3.heater_active_time;
    case SVC_FAN:
      return in3.fan_active_time;
    case SVC_HUMIDIFIER:
      return in3.humidifier_active_time;
    case SVC_PHOTO:
      return in3.phototherapy_active_time;
    default:
      return 0.0f;
  }
}

float s_base[SVC_COUNT] = {0.0f, 0.0f, 0.0f, 0.0f};

}  // namespace

void ServiceHours_Init(void) {
  Preferences p;
  if (!p.begin(NS_RT, true)) {
    ESP_LOGE(TAG, "no se pudo abrir %s: bases a 0", NS_RT);
    return;
  }
  for (uint8_t i = 0; i < SVC_COUNT; i++) {
    s_base[i] = p.getFloat(KEY_BASE[i], 0.0f);
  }
  p.end();
  ESP_LOGI(TAG, "bases: heater=%.1f fan=%.1f hum=%.1f photo=%.1f", s_base[0],
           s_base[1], s_base[2], s_base[3]);
}

void ServiceHours_BuildReport(ServiceReport *out) {
  float life[SVC_COUNT];
  uint32_t thr[SVC_COUNT];
  for (uint8_t i = 0; i < SVC_COUNT; i++) {
    life[i] = lifeHours(i);
    thr[i] = service_threshold_hours(i);
  }
  service_calc_report(life, s_base, thr, out);
}

uint32_t ServiceHours_SinceHours(uint8_t id) {
  if (id >= SVC_COUNT) return 0;
  return service_whole_hours(service_hours_since(lifeHours(id), s_base[id]));
}

bool ServiceHours_MarkDone(uint8_t id, uint32_t epochUtc) {
  if (id >= SVC_COUNT) return false;
  const float life = lifeHours(id);
  Preferences p;
  if (!p.begin(NS_RT, false)) {
    ESP_LOGE(TAG, "servicio %u: no se pudo abrir NVS", (unsigned)id);
    return false;
  }
  // putFloat/putUInt devuelven 0 si fallan. La base se confirma al final: si
  // la escritura falla no se cambia la copia en RAM, para no mostrar un
  // servicio "hecho" que un reinicio deshace.
  const size_t wb = p.putFloat(KEY_BASE[id], life);
  const size_t we = p.putUInt(KEY_EPOCH[id], epochUtc);
  p.end();
  if (wb == 0 || we == 0) {
    ESP_LOGE(TAG, "servicio %u: fallo al escribir NVS", (unsigned)id);
    return false;
  }
  s_base[id] = life;
  ESP_LOGI(TAG, "servicio %u registrado: base=%.1f h epoch=%lu", (unsigned)id,
           life, (unsigned long)epochUtc);
  return true;
}

#include "service_settings.h"

#include <Preferences.h>

#include "EEPROM_defines.h"  // HMI_NS_CFG, HMI_KEY_SVC_EN
#include "esp_log.h"
#include "service_state.h"

namespace {
constexpr const char *TAG = "SVC";
}

void ServiceSettings_Init(void) {
  Preferences p;
  if (!p.begin(HMI_NS_CFG, true)) {
    ESP_LOGE(TAG, "no se pudo abrir NVS: aviso de servicio ACTIVADO por defecto");
    ServiceState_SetEnabled(true);
    return;
  }
  // Defecto 1 (ON): tambien en una NVS virgen.
  ServiceState_SetEnabled(p.getUChar(HMI_KEY_SVC_EN, 1) != 0);
  p.end();
  ESP_LOGI(TAG, "aviso de servicio %s",
           ServiceState_IsEnabled() ? "activado" : "desactivado");
}

void ServiceSettings_SetEnabled(bool on) {
  if (on == ServiceState_IsEnabled()) return;
  Preferences p;
  if (!p.begin(HMI_NS_CFG, false)) {
    ESP_LOGE(TAG, "no se pudo abrir NVS: el interruptor no cambia");
    return;
  }
  const size_t w = p.putUChar(HMI_KEY_SVC_EN, on ? 1 : 0);
  p.end();
  if (w == 0) {
    ESP_LOGE(TAG, "fallo al guardar svc_en: el interruptor no cambia");
    return;
  }
  ServiceState_SetEnabled(on);
}

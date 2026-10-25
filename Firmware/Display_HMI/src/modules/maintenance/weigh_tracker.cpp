#include "modules/maintenance/weigh_tracker.h"

#include <Preferences.h>

#include "CommTask.h"        // HMI_GetEpochNow()
#include "EEPROM_defines.h"  // HMI_NS_CFG, HMI_KEY_WGH_*
#include "esp_log.h"
#include "modules/maintenance/weigh_state.h"
#include "state/training_mode.h"  // Training_IsActive(), Training_IsPracticeSeq()
#include "ui/BabyWizard.h"        // BabyWizard_GetActiveSeq(), HasLiveSession()

// Cola fina: la maquina de estados (alta, cambio de bebe, sello pendiente,
// reancla) vive en weigh_state.cpp y se prueba en host. Aqui solo se carga y
// guarda en NVS y se rellena el entorno con el reloj, BabyWizard y formacion.

namespace {

constexpr const char *TAG = "WGH";

weigh_state_t s_state = [] {
  weigh_state_t s;
  weigh_state_init(&s);
  return s;
}();

// False si Init() no pudo leer la NVS. Entonces el estado en RAM es el de
// fabrica, no lo que hubiera guardado, y escribirlo pisaria valores buenos con
// ceros: Flush() no hace nada.
bool s_loaded = false;

weigh_env_t makeEnv() {
  weigh_env_t e = {};
  e.nowEpoch = HMI_GetEpochNow();
  e.activeSeq = BabyWizard_GetActiveSeq();
  e.training = Training_IsActive();
  return e;
}

}  // namespace

void WeighTracker_Init(void) {
  weigh_state_init(&s_state);
  s_loaded = false;

  Preferences p;
  // Lectura primero. Con el espacio de nombres aun sin crear (NVS virgen),
  // begin() de solo lectura falla aunque la NVS este sana: se reintenta en
  // escritura, que lo crea. Solo si tambien falla es un error de verdad.
  if (!p.begin(HMI_NS_CFG, true)) {
    if (!p.begin(HMI_NS_CFG, false)) {
      ESP_LOGW(TAG, "init: no se pudo abrir la NVS; no se escribira nada");
      return;
    }
  }
  s_state.weighSeq = p.getUInt(HMI_KEY_WGH_SEQ, 0);
  s_state.weighEpoch = p.getUInt(HMI_KEY_WGH_LAST, 0);
  s_state.seenSeq = p.getUInt(HMI_KEY_WGH_SEEN_SEQ, 0);
  s_state.seenEpoch = p.getUInt(HMI_KEY_WGH_SEEN_AT, 0);
  s_state.snoozeUntil = p.getUInt(HMI_KEY_WGH_SNOOZE, 0);
  s_state.enabled = p.getUChar(HMI_KEY_WGH_EN, 1) != 0;  // por defecto ON
  p.end();
  s_loaded = true;
  ESP_LOGI(TAG,
           "init: en=%d weigh seq=%lu at=%lu seen seq=%lu at=%lu snooze=%lu",
           (int)s_state.enabled, (unsigned long)s_state.weighSeq,
           (unsigned long)s_state.weighEpoch, (unsigned long)s_state.seenSeq,
           (unsigned long)s_state.seenEpoch, (unsigned long)s_state.snoozeUntil);
}

void WeighTracker_Tick(void) {
  const weigh_env_t env = makeEnv();
  weigh_state_tick(&s_state, &env);
}

void WeighTracker_NoteWeighed(uint32_t seq, uint16_t grams) {
  const weigh_env_t env = makeEnv();
  if (weigh_state_note_weighed(&s_state, &env, seq, grams,
                               Training_IsPracticeSeq(seq))) {
    ESP_LOGI(TAG, "pesada: bebe %lu, %s", (unsigned long)seq,
             env.nowEpoch != 0 ? "sellada" : "pendiente de hora");
  }
}

bool WeighTracker_ShouldWarn(void) {
  const weigh_env_t env = makeEnv();
  return weigh_state_should_warn(&s_state, &env, BabyWizard_HasLiveSession());
}

uint32_t WeighTracker_ElapsedHours(void) {
  const weigh_env_t env = makeEnv();
  const weigh_inputs_t in =
      weigh_state_inputs(&s_state, &env, BabyWizard_HasLiveSession());
  return weigh_elapsed_hours(&in);
}

void WeighTracker_Snooze(void) {
  if (!weigh_state_snooze(&s_state, HMI_GetEpochNow())) {
    ESP_LOGI(TAG, "mas tarde sin hora valida: no se puede fechar el silencio");
  }
}

bool WeighTracker_IsEnabled(void) { return s_state.enabled; }

void WeighTracker_SetEnabled(bool on) {
  weigh_state_set_enabled(&s_state, on);
  ESP_LOGI(TAG, "aviso de pesada %s", on ? "activado" : "desactivado");
}

void WeighTracker_Flush(void) {
  if (!s_state.dirty) return;
  if (!s_loaded) {
    // Ver s_loaded. Se descarta lo pendiente: en RAM sigue valiendo para esta
    // sesion y reintentar cada vuelta solo llenaria el log.
    s_state.dirty = false;
    return;
  }
  Preferences p;
  if (!p.begin(HMI_NS_CFG, false)) {
    // Se queda sucio y se reintenta en la siguiente vuelta.
    ESP_LOGW(TAG, "flush: no se pudo abrir la NVS");
    return;
  }
  bool ok = true;
  ok &= p.putUInt(HMI_KEY_WGH_SEQ, s_state.weighSeq) == sizeof(uint32_t);
  ok &= p.putUInt(HMI_KEY_WGH_LAST, s_state.weighEpoch) == sizeof(uint32_t);
  ok &= p.putUInt(HMI_KEY_WGH_SEEN_SEQ, s_state.seenSeq) == sizeof(uint32_t);
  ok &= p.putUInt(HMI_KEY_WGH_SEEN_AT, s_state.seenEpoch) == sizeof(uint32_t);
  ok &= p.putUInt(HMI_KEY_WGH_SNOOZE, s_state.snoozeUntil) == sizeof(uint32_t);
  ok &= p.putUChar(HMI_KEY_WGH_EN, s_state.enabled ? 1 : 0) == sizeof(uint8_t);
  p.end();
  if (ok) {
    s_state.dirty = false;
  } else {
    ESP_LOGW(TAG, "flush: escritura NVS fallida, se reintenta");
  }
}

#include "state/patient_state.h"

#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>

#include "CommTask.h"        // HMI_GetEpochNow()
#include "EEPROM_defines.h"  // HMI_NS_CFG, HMI_KEY_PAT_*
#include "esp_log.h"
#include "state/training_mode.h"  // Training_IsActive(), Training_IsPracticeSeq()
#include "ui/BabyWizard.h"        // BabyWizard_SetSession()

namespace {

constexpr const char *TAG = "PATIENT";

PatientIdentity s_id = {};  // seq 0 = nadie
bool s_idDirty = false;
bool s_erasePending = false;
uint32_t s_idRetryAtMs = 0;
constexpr uint32_t PATIENT_NVS_RETRY_MS = 5000u;

// En formacion nada de esto cuenta: el bebe de practicas no es un paciente.
bool ignoredNow(uint32_t seq) {
  return Training_IsActive() || (seq != 0 && Training_IsPracticeSeq(seq));
}

}  // namespace

void PatientState_Restore(void) {
  Preferences p;
  if (!p.begin(HMI_NS_CFG, true)) return;  // NVS virgen: nada que restaurar
  uint8_t blob[PATIENT_ID_RECORD_SIZE];
  const size_t have = p.getBytesLength(HMI_KEY_PAT_ID);
  size_t got = 0;
  if (have == sizeof(blob)) got = p.getBytes(HMI_KEY_PAT_ID, blob, sizeof(blob));
  p.end();
  if (got != sizeof(blob)) {
    if (have != 0) ESP_LOGW(TAG, "pat_id con tamano %u: descartado", (unsigned)have);
    return;
  }

  PatientIdentity id;
  if (!patient_identity_decode(blob, sizeof(blob), &id)) {
    ESP_LOGW(TAG, "pat_id invalido: descartado");
    return;
  }
  // Primero el estado propio, luego el asistente: asi el gancho de SetSession
  // ve el mismo seq y no lo toma por un bebe nuevo.
  s_id = id;
  BabyWizardSession s = {};
  s.seq = id.seq;
  snprintf(s.name, sizeof(s.name), "%s", id.name);
  s.gest = id.gestWeeks;
  s.weight = 0;
  BabyWizard_SetSession(&s);
  ESP_LOGI(TAG, "restaurado seq=%lu gest=%u age=%d", (unsigned long)id.seq,
           (unsigned)id.gestWeeks, (int)id.ageKnown);
}

void PatientState_OnSessionSet(uint32_t seq, const char *name,
                               uint8_t gestWeeks) {
  if (seq == 0) {
    PatientState_OnClear();
    return;
  }
  if (ignoredNow(seq)) return;
  const char *n = name ? name : "";
  if (s_id.seq == seq) {
    // Mismo bebe: la edad capturada sigue valiendo.
    if (s_id.gestWeeks != gestWeeks || strncmp(s_id.name, n, PATIENT_NAME_LEN) != 0) {
      s_id.gestWeeks = gestWeeks;
      snprintf(s_id.name, sizeof(s_id.name), "%s", n);
      s_idDirty = true;
    }
    return;
  }
  memset(&s_id, 0, sizeof(s_id));
  s_id.seq = seq;
  snprintf(s_id.name, sizeof(s_id.name), "%s", n);
  s_id.gestWeeks = gestWeeks;
  s_idDirty = true;
}

void PatientState_OnAge(uint32_t seq, uint16_t ageDays, bool known) {
  if (ignoredNow(seq) || seq == 0 || s_id.seq != seq) return;
  s_id.ageKnown = known;
  s_id.ageDays = known ? ageDays : 0;
  s_id.captureEpoch = known ? HMI_GetEpochNow() : 0;
  s_idDirty = true;
}

void PatientState_OnClear(void) {
  if (Training_IsActive()) return;
  if (s_id.seq == 0) return;
  memset(&s_id, 0, sizeof(s_id));
  s_idDirty = false;
  s_erasePending = true;
}

PatientAge PatientState_GetAge(uint32_t seq, uint8_t gestWeeks) {
  if (seq == 0 || s_id.seq != seq) return PatientAge{false, 0, false, 0, 0};
  return patient_age_compute(gestWeeks, s_id.ageKnown, s_id.ageDays,
                             s_id.captureEpoch, HMI_GetEpochNow());
}

void PatientState_Service(void) {
  if (s_erasePending) {
    s_erasePending = false;
    Preferences p;
    if (p.begin(HMI_NS_CFG, false)) {
      p.remove(HMI_KEY_PAT_ID);
      p.remove(HMI_KEY_PAT_THER);
      p.end();
    }
    return;
  }
  if (s_idDirty && s_id.seq != 0 &&
      (int32_t)(millis() - s_idRetryAtMs) >= 0) {
    s_idDirty = false;
    uint8_t blob[PATIENT_ID_RECORD_SIZE];
    if (!patient_identity_encode(&s_id, blob, sizeof(blob))) return;
    Preferences p;
    size_t wrote = 0;
    if (p.begin(HMI_NS_CFG, false)) {
      wrote = p.putBytes(HMI_KEY_PAT_ID, blob, sizeof(blob));
      p.end();
    }
    if (wrote != sizeof(blob)) {
      // Se reintenta, pero no en cada pasada de UI_Task (unos pocos ms).
      ESP_LOGE(TAG, "pat_id: escritura fallida (%u)", (unsigned)wrote);
      s_idDirty = true;
      s_idRetryAtMs = millis() + PATIENT_NVS_RETRY_MS;
    }
  }
}

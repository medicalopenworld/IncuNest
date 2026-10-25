#include "state/patient_state.h"

#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>

#include "CommTask.h"        // HMI_GetEpochNow(), ctrl_state_msg, enlace
#include "EEPROM_defines.h"  // HMI_NS_CFG, HMI_KEY_PAT_*
#include "esp_log.h"
#include "state/training_mode.h"  // Training_IsActive(), Training_IsPracticeSeq()
#include "ui/BabyWizard.h"        // BabyWizard_SetSession()

namespace {

constexpr const char *TAG = "PATIENT";

constexpr uint32_t SERVICE_PERIOD_MS = 1000u;
constexpr uint32_t NVS_RETRY_MS = 5000u;

PatientIdentity s_id = {};  // seq 0 = nadie
bool s_idDirty = false;
bool s_erasePending = false;
// Ultimo fallo de escritura de la identidad. Se guarda el INSTANTE del fallo y
// no un "reintentar a las": comparar contra un plazo absoluto con millis() se
// rompe al pasar de 24,8 dias de funcionamiento (int32 con signo).
bool s_idWriteFailed = false;
uint32_t s_idFailedAtMs = 0;

PatientAccum s_acc = {};  // seq 0 = nadie
uint32_t s_mirrorGen = 0;
uint32_t s_lastServiceMs = 0;
bool s_serviceStarted = false;

// En formacion nada de esto cuenta: el bebe de practicas no es un paciente.
bool ignoredNow(uint32_t seq) {
  return Training_IsActive() || (seq != 0 && Training_IsPracticeSeq(seq));
}

bool identityRetryAllowed() {
  return !s_idWriteFailed || (millis() - s_idFailedAtMs) >= NVS_RETRY_MS;
}

void writeIdentity() {
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
    s_idWriteFailed = true;
    s_idFailedAtMs = millis();
    return;
  }
  s_idWriteFailed = false;
}

void writeTherapy(uint32_t nowMs) {
  uint8_t blob[PATIENT_THER_RECORD_SIZE];
  if (!patient_therapy_encode(&s_acc, blob, sizeof(blob))) {
    patient_accum_flush_done(&s_acc, nowMs);
    return;
  }
  Preferences p;
  size_t wrote = 0;
  if (p.begin(HMI_NS_CFG, false)) {
    wrote = p.putBytes(HMI_KEY_PAT_THER, blob, sizeof(blob));
    p.end();
  }
  patient_accum_flush_done(&s_acc, nowMs);
  if (wrote != sizeof(blob)) {
    ESP_LOGE(TAG, "pat_ther: escritura fallida (%u)", (unsigned)wrote);
    s_acc.dirty = true;  // pendiente; lastFlushMs ya espacia el reintento
  }
}

// Resincroniza con la ultima PROFILE_LIST de la placa, si trajo al bebe.
void resyncFromMirror() {
  BabyProfileListMsg m;
  if (!Communication_TakeProfileListMirror(&s_mirrorGen, &m)) return;
  if (s_acc.seq == 0) return;
  for (int i = 0; i < m.count && i < 3; i++) {
    if (m.items[i].seq != s_acc.seq) continue;
    const PatientMinutes board = {m.items[i].phototherapyMinutes,
                                  m.items[i].thermoMinutes,
                                  m.items[i].humidityMinutes};
    patient_accum_resync(&s_acc, s_acc.seq, &board);
    return;
  }
}

}  // namespace

void PatientState_Restore(void) {
  Preferences p;
  if (!p.begin(HMI_NS_CFG, true)) return;  // NVS virgen: nada que restaurar
  uint8_t idBlob[PATIENT_ID_RECORD_SIZE];
  uint8_t thBlob[PATIENT_THER_RECORD_SIZE];
  const size_t idHave = p.getBytesLength(HMI_KEY_PAT_ID);
  const size_t thHave = p.getBytesLength(HMI_KEY_PAT_THER);
  size_t idGot = 0, thGot = 0;
  if (idHave == sizeof(idBlob)) {
    idGot = p.getBytes(HMI_KEY_PAT_ID, idBlob, sizeof(idBlob));
  }
  if (thHave == sizeof(thBlob)) {
    thGot = p.getBytes(HMI_KEY_PAT_THER, thBlob, sizeof(thBlob));
  }
  p.end();

  PatientIdentity id;
  if (idGot != sizeof(idBlob) ||
      !patient_identity_decode(idBlob, sizeof(idBlob), &id)) {
    if (idHave != 0) ESP_LOGW(TAG, "pat_id invalido (%u B): descartado", (unsigned)idHave);
    return;
  }

  // Primero el estado propio, luego el asistente: asi el gancho de SetSession
  // ve el mismo seq y no lo toma por un bebe nuevo.
  s_id = id;
  patient_accum_begin(&s_acc, id.seq, nullptr);
  PatientTherapyRecord rec;
  bool therapyRestored = false;
  if (thGot == sizeof(thBlob) &&
      patient_therapy_decode(thBlob, sizeof(thBlob), &rec)) {
    // Solo vale si es del mismo bebe: dos blobs, dos escrituras, y un corte
    // entre ambas puede dejar acumulados de otro.
    therapyRestored = patient_accum_restore(&s_acc, &rec, id.seq);
  }
  if (!therapyRestored) {
    patient_accum_flush_done(&s_acc, 0);  // nada nuevo que volcar
  }

  BabyWizardSession s = {};
  s.seq = id.seq;
  snprintf(s.name, sizeof(s.name), "%s", id.name);
  s.gest = id.gestWeeks;
  s.weight = 0;
  BabyWizard_SetSession(&s);
  const PatientMinutes m = patient_accum_minutes(&s_acc);
  ESP_LOGI(TAG, "restaurado seq=%lu gest=%u age=%d ther=%d (%lu/%lu/%lu min)",
           (unsigned long)id.seq, (unsigned)id.gestWeeks, (int)id.ageKnown,
           (int)therapyRestored, (unsigned long)m.photo, (unsigned long)m.thermo,
           (unsigned long)m.humidity);
}

void PatientState_OnSessionSet(uint32_t seq, const char *name,
                               uint8_t gestWeeks, const PatientMinutes *seed) {
  if (seq == 0) {
    PatientState_OnClear();
    return;
  }
  if (ignoredNow(seq)) return;
  const char *n = name ? name : "";
  if (s_id.seq == seq) {
    // Mismo bebe: la edad y los acumulados ya capturados siguen valiendo (el
    // contador local es mas fresco que cualquier lista vieja).
    if (s_id.gestWeeks != gestWeeks ||
        strncmp(s_id.name, n, PATIENT_NAME_LEN) != 0) {
      s_id.gestWeeks = gestWeeks;
      snprintf(s_id.name, sizeof(s_id.name), "%s", n);
      s_idDirty = true;
    }
    if (s_acc.seq != seq) patient_accum_begin(&s_acc, seq, seed);
    return;
  }
  memset(&s_id, 0, sizeof(s_id));
  s_id.seq = seq;
  snprintf(s_id.name, sizeof(s_id.name), "%s", n);
  s_id.gestWeeks = gestWeeks;
  s_idDirty = true;
  patient_accum_begin(&s_acc, seq, seed);
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
  if (s_id.seq == 0 && s_acc.seq == 0) return;
  memset(&s_id, 0, sizeof(s_id));
  patient_accum_init(&s_acc);
  s_idDirty = false;
  s_erasePending = true;
}

PatientAge PatientState_GetAge(uint32_t seq, uint8_t gestWeeks) {
  if (seq == 0 || s_id.seq != seq) return PatientAge{false, 0, false, 0, 0};
  return patient_age_compute(gestWeeks, s_id.ageKnown, s_id.ageDays,
                             s_id.captureEpoch, HMI_GetEpochNow());
}

bool PatientState_GetTherapy(uint32_t seq, PatientMinutes *out) {
  if (!out || seq == 0 || s_acc.seq != seq) return false;
  *out = patient_accum_minutes(&s_acc);
  return true;
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

  if (s_idDirty && s_id.seq != 0 && identityRetryAllowed()) writeIdentity();

  // El resto es el reloj de terapia: ~1 Hz, no en cada pasada de UI_Task.
  const uint32_t now = millis();
  if (s_serviceStarted && (now - s_lastServiceMs) < SERVICE_PERIOD_MS) return;
  s_serviceStarted = true;
  s_lastServiceMs = now;

  // Formacion o enlace caido: ctrl_state_msg esta virtualizado u obsoleto y no
  // dice nada de lo que la incubadora esta haciendo AHORA.
  const bool linkOk = !Training_IsActive() && !Display_IsBoardLinkLost();
  patient_accum_tick(&s_acc, now, (uint8_t)ctrl_state_msg.actuation,
                     ctrl_state_msg.phototherapyMode != 0, linkOk);
  resyncFromMirror();

  // Un fallo de escritura deja `dirty` y, como flush_done() ya anoto la hora,
  // el siguiente intento espera un intervalo entero: no hay bucle de reintentos.
  if (patient_accum_flush_due(&s_acc, now)) writeTherapy(now);
}

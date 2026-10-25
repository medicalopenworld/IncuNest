// RED: stubs que compilan y no hacen nada. La implementacion llega en el
// commit siguiente; los tests de test/test_patient_session deben fallar por
// comportamiento, no por enlace.
#include "patient_session.h"

PatientAge patient_age_compute(uint8_t, bool, uint16_t, uint32_t, uint32_t) {
  return PatientAge{false, 0, false, 0, 0};
}

size_t patient_fmt_minutes(uint32_t, char *out, size_t len) {
  if (out && len) out[0] = '\0';
  return 0;
}

void patient_accum_init(PatientAccum *) {}
void patient_accum_begin(PatientAccum *, uint32_t, const PatientMinutes *) {}
bool patient_accum_tick(PatientAccum *, uint32_t, uint8_t, bool, bool) {
  return false;
}
PatientMinutes patient_accum_minutes(const PatientAccum *) {
  return PatientMinutes{0, 0, 0};
}
bool patient_accum_resync(PatientAccum *, uint32_t, const PatientMinutes *) {
  return false;
}
bool patient_accum_flush_due(const PatientAccum *, uint32_t) { return false; }
void patient_accum_flush_done(PatientAccum *, uint32_t) {}

bool patient_identity_encode(const PatientIdentity *, uint8_t *, size_t) {
  return false;
}
bool patient_identity_decode(const uint8_t *, size_t, PatientIdentity *) {
  return false;
}
bool patient_therapy_encode(const PatientAccum *, uint8_t *, size_t) {
  return false;
}
bool patient_therapy_decode(const uint8_t *, size_t, PatientTherapyRecord *) {
  return false;
}
bool patient_accum_restore(PatientAccum *, const PatientTherapyRecord *,
                           uint32_t) {
  return false;
}

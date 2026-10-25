#include "patient_session.h"

#include <stdio.h>
#include <string.h>

namespace {

constexpr uint32_t SECONDS_PER_DAY = 86400u;
constexpr uint32_t MAX_MINUTES_AS_SECONDS = 0xFFFFFFFFu / 60u;

uint32_t minutes_to_seconds(uint32_t minutes) {
  return minutes > MAX_MINUTES_AS_SECONDS ? 0xFFFFFFFFu : minutes * 60u;
}

uint32_t sat_add(uint32_t a, uint32_t b) {
  const uint32_t s = a + b;
  return s < a ? 0xFFFFFFFFu : s;
}

void put_u16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)(v >> 8);
}
void put_u32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
  p[2] = (uint8_t)((v >> 16) & 0xFFu);
  p[3] = (uint8_t)(v >> 24);
}
uint16_t get_u16(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}
uint32_t get_u32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

// Offsets del registro de identidad (ver patient_session.h).
constexpr size_t ID_VER = 0, ID_GEST = 1, ID_FLAGS = 2, ID_SEQ = 4,
                 ID_EPOCH = 8, ID_AGE = 12, ID_NAME = 16, ID_SN = 40;
constexpr uint8_t ID_FLAG_AGE_KNOWN = 0x01;
static_assert(ID_NAME + PATIENT_NAME_LEN == ID_SN && ID_SN + 4 == PATIENT_ID_RECORD_SIZE,
              "el sn cierra el registro de identidad");

// Offsets del registro de terapia.
constexpr size_t TH_VER = 0, TH_SEQ = 4, TH_PHOTO = 8, TH_THERMO = 12,
                 TH_HUM = 16;
static_assert(TH_HUM + 4 == PATIENT_THER_RECORD_SIZE,
              "la humedad cierra el registro de terapia");

bool any_on(const PatientAccum *a) {
  return a->photoOn || a->thermoOn || a->humidityOn;
}

}  // namespace

// ------------------------------------------------------------------- edad

PatientAge patient_age_compute(uint8_t gestWeeks, bool ageKnown,
                               uint16_t ageDaysAtCapture,
                               uint32_t captureEpoch, uint32_t nowEpoch) {
  PatientAge out = {false, 0, false, 0, 0};
  if (!ageKnown) return out;

  // Sin hora (al capturar o ahora) no se puede afirmar cuantos dias han
  // pasado: desconocida, no "el valor capturado".
  if (captureEpoch == 0 || nowEpoch == 0) return out;

  uint32_t elapsedDays = 0;
  if (nowEpoch >= captureEpoch) {
    elapsedDays = (nowEpoch - captureEpoch) / SECONDS_PER_DAY;
  }
  if (elapsedDays > PATIENT_AGE_MAX_ELAPSED_DAYS) return out;
  out.hasLifeDays = true;
  out.lifeDays = (uint32_t)ageDaysAtCapture + elapsedDays;

  if (gestWeeks > 0) {
    const uint32_t totalDays = (uint32_t)gestWeeks * 7u + out.lifeDays;
    out.hasCorrected = true;
    out.corrWeeks = totalDays / 7u;
    out.corrDays = (uint8_t)(totalDays % 7u);
  }
  return out;
}

// ---------------------------------------------------------------- formato

size_t patient_fmt_life_days(uint32_t days, char *out, size_t len) {
  if (!out || len == 0) return 0;
  const int n = days > PATIENT_LIFE_DAYS_CAP
                    ? snprintf(out, len, "%u+", (unsigned)PATIENT_LIFE_DAYS_CAP)
                    : snprintf(out, len, "%u", (unsigned)days);
  if (n < 0) {
    out[0] = '\0';
    return 0;
  }
  return (size_t)n < len ? (size_t)n : len - 1;
}

size_t patient_fmt_minutes(uint32_t minutes, char *out, size_t len) {
  if (!out || len == 0) return 0;
  int n;
  if (minutes < 60u) {
    n = snprintf(out, len, "%um", (unsigned)minutes);
  } else if (minutes % 60u == 0u) {
    n = snprintf(out, len, "%uh", (unsigned)(minutes / 60u));
  } else {
    n = snprintf(out, len, "%uh %um", (unsigned)(minutes / 60u),
                 (unsigned)(minutes % 60u));
  }
  if (n < 0) {
    out[0] = '\0';
    return 0;
  }
  return (size_t)n < len ? (size_t)n : len - 1;
}

// ------------------------------------------------------------- acumulador

void patient_accum_init(PatientAccum *a) {
  if (!a) return;
  memset(a, 0, sizeof(*a));
}

void patient_accum_begin(PatientAccum *a, uint32_t seq,
                         const PatientMinutes *seed) {
  if (!a) return;
  patient_accum_init(a);
  if (seq == 0) return;
  a->seq = seq;
  if (seed) {
    a->known = true;
    a->photoSec = minutes_to_seconds(seed->photo);
    a->thermoSec = minutes_to_seconds(seed->thermo);
    a->humiditySec = minutes_to_seconds(seed->humidity);
  }
  a->dirty = true;
}

PatientMinutes patient_accum_minutes(const PatientAccum *a) {
  PatientMinutes m = {0, 0, 0};
  if (!a) return m;
  m.photo = a->photoSec / 60u;
  m.thermo = a->thermoSec / 60u;
  m.humidity = a->humiditySec / 60u;
  return m;
}

bool patient_accum_tick(PatientAccum *a, uint32_t nowMs, uint8_t act,
                        bool photo, bool linkOk) {
  if (!a) return false;
  const PatientMinutes before = patient_accum_minutes(a);
  const bool wasAny = any_on(a);

  // Con enlace caido el CTRL,STATE guardado esta obsoleto: no dice nada de lo
  // que pasa ahora. Y sin bebe no hay a quien acreditar (igual que la placa).
  const bool track = (a->seq != 0) && linkOk;
  a->photoOn = track && photo;
  a->thermoOn = track && (act == PATIENT_ACT_TEMPERATURE ||
                          act == PATIENT_ACT_TEMP_AND_HUMIDITY);
  a->humidityOn = track && (act == PATIENT_ACT_HUMIDITY ||
                            act == PATIENT_ACT_TEMP_AND_HUMIDITY);

  uint32_t credit = 0;
  if (a->hasTick && track && a->known) {
    const uint32_t dt = nowMs - a->lastTickMs;  // aritmetica modular: wrap-safe
    credit = dt > PATIENT_TICK_MAX_CREDIT_MS ? PATIENT_TICK_MAX_CREDIT_MS : dt;
  }
  a->lastTickMs = nowMs;
  a->hasTick = true;

  const uint32_t total = a->remMs + credit;
  const uint32_t whole = total / 1000u;
  a->remMs = total % 1000u;
  if (whole > 0) {
    if (a->photoOn) a->photoSec = sat_add(a->photoSec, whole);
    if (a->thermoOn) a->thermoSec = sat_add(a->thermoSec, whole);
    if (a->humidityOn) a->humiditySec = sat_add(a->humiditySec, whole);
    if (any_on(a)) a->dirty = true;
  }
  if (wasAny && !any_on(a)) a->stopEdge = true;

  const PatientMinutes after = patient_accum_minutes(a);
  return before.photo != after.photo || before.thermo != after.thermo ||
         before.humidity != after.humidity;
}

namespace {
// Devuelve true si cambia el valor MOSTRADO (minutos). Conserva los segundos
// sueltos cuando los minutos ya coinciden.
bool resync_one(uint32_t *sec, bool running, uint32_t boardMinutes) {
  const uint32_t boardSec = minutes_to_seconds(boardMinutes);
  const uint32_t target = (running && *sec > boardSec) ? *sec : boardSec;
  if (target / 60u == *sec / 60u) return false;
  *sec = target;
  return true;
}
}  // namespace

bool patient_accum_resync(PatientAccum *a, uint32_t seq,
                          const PatientMinutes *board) {
  if (!a || !board || seq == 0 || seq != a->seq) return false;
  bool changed = false;
  const bool wasKnown = a->known;
  changed |= resync_one(&a->photoSec, wasKnown && a->photoOn, board->photo);
  changed |= resync_one(&a->thermoSec, wasKnown && a->thermoOn, board->thermo);
  changed |= resync_one(&a->humiditySec, wasKnown && a->humidityOn, board->humidity);
  if (!wasKnown) {
    a->known = true;
    changed = true;
  }
  if (changed) a->dirty = true;
  return changed;
}

bool patient_accum_flush_due(const PatientAccum *a, uint32_t nowMs) {
  if (!a || !a->dirty) return false;
  return a->stopEdge || (nowMs - a->lastFlushMs) >= PATIENT_FLUSH_INTERVAL_MS;
}

void patient_accum_flush_done(PatientAccum *a, uint32_t nowMs) {
  if (!a) return;
  a->dirty = false;
  a->stopEdge = false;
  a->lastFlushMs = nowMs;
}

// ---------------------------------------------------------- registros NVS

bool patient_identity_encode(const PatientIdentity *id, uint8_t *out,
                             size_t outLen) {
  if (!id || !out || outLen < PATIENT_ID_RECORD_SIZE || id->seq == 0) {
    return false;
  }
  memset(out, 0, PATIENT_ID_RECORD_SIZE);
  out[ID_VER] = PATIENT_ID_RECORD_VERSION;
  out[ID_GEST] = id->gestWeeks;
  out[ID_FLAGS] = id->ageKnown ? ID_FLAG_AGE_KNOWN : 0;
  put_u32(out + ID_SEQ, id->seq);
  put_u32(out + ID_EPOCH, id->captureEpoch);
  put_u16(out + ID_AGE, id->ageDays);
  put_u32(out + ID_SN, id->boardSn);
  // Siempre termina en NUL dentro de los 24 bytes, aunque `name` no lo haga.
  size_t n = 0;
  while (n < PATIENT_NAME_LEN - 1 && id->name[n] != '\0') n++;
  memcpy(out + ID_NAME, id->name, n);
  return true;
}

bool patient_identity_decode(const uint8_t *in, size_t len,
                             PatientIdentity *id) {
  if (!in || !id || len != PATIENT_ID_RECORD_SIZE) return false;
  if (in[ID_VER] != PATIENT_ID_RECORD_VERSION) return false;
  const uint32_t seq = get_u32(in + ID_SEQ);
  if (seq == 0) return false;
  if (memchr(in + ID_NAME, '\0', PATIENT_NAME_LEN) == nullptr) return false;
  // Semanas: 0 (no informadas) o dentro de lo que admite el asistente. Un byte
  // corrupto no debe llegar a una edad corregida de cientos de semanas.
  const uint8_t gest = in[ID_GEST];
  if (gest != 0 && (gest < PATIENT_GEST_MIN || gest > PATIENT_GEST_MAX)) {
    return false;
  }

  id->seq = seq;
  id->gestWeeks = in[ID_GEST];
  id->ageKnown = (in[ID_FLAGS] & ID_FLAG_AGE_KNOWN) != 0;
  id->ageDays = get_u16(in + ID_AGE);
  id->captureEpoch = get_u32(in + ID_EPOCH);
  id->boardSn = get_u32(in + ID_SN);
  memcpy(id->name, in + ID_NAME, PATIENT_NAME_LEN);
  return true;
}

bool patient_therapy_encode(const PatientAccum *a, uint8_t *out,
                            size_t outLen) {
  if (!a || !out || outLen < PATIENT_THER_RECORD_SIZE || a->seq == 0) {
    return false;
  }
  memset(out, 0, PATIENT_THER_RECORD_SIZE);
  out[TH_VER] = PATIENT_THER_RECORD_VERSION;
  put_u32(out + TH_SEQ, a->seq);
  put_u32(out + TH_PHOTO, a->photoSec);
  put_u32(out + TH_THERMO, a->thermoSec);
  put_u32(out + TH_HUM, a->humiditySec);
  return true;
}

bool patient_therapy_decode(const uint8_t *in, size_t len,
                            PatientTherapyRecord *rec) {
  if (!in || !rec || len != PATIENT_THER_RECORD_SIZE) return false;
  if (in[TH_VER] != PATIENT_THER_RECORD_VERSION) return false;
  const uint32_t seq = get_u32(in + TH_SEQ);
  if (seq == 0) return false;
  rec->seq = seq;
  rec->photoSec = get_u32(in + TH_PHOTO);
  rec->thermoSec = get_u32(in + TH_THERMO);
  rec->humiditySec = get_u32(in + TH_HUM);
  return true;
}

bool patient_accum_restore(PatientAccum *a, const PatientTherapyRecord *rec,
                           uint32_t expectedSeq) {
  if (!a || !rec || expectedSeq == 0 || rec->seq != expectedSeq) return false;
  patient_accum_init(a);
  a->seq = rec->seq;
  a->photoSec = rec->photoSec;
  a->thermoSec = rec->thermoSec;
  a->humiditySec = rec->humiditySec;
  a->known = true;
  return true;
}

// ------------------------------------------- confirmacion con la placa

PatientConfirmAction patient_confirm_decide(uint32_t identitySeq,
                                            uint32_t identitySn,
                                            const PatientBoardView *board) {
  if (!board || identitySeq == 0 || !board->stateSeen) {
    return PATIENT_CONFIRM_WAIT;
  }
  // Otra placa: los seq no significan nada aqui, sea cual sea el resto.
  if (board->sn != identitySn) return PATIENT_CONFIRM_CLEAR;
  // Placa antigua: no hay forma de confirmar por si sola (se contrasta con la
  // lista). Su activeSeq no viaja, asi que no se mira.
  if (!board->sendsActiveSeq) return PATIENT_CONFIRM_WAIT;
  if (board->activeSeq == identitySeq) return PATIENT_CONFIRM_CONFIRM;
  if (board->activeSeq != 0) return PATIENT_CONFIRM_CLEAR;
  // activeSeq == 0: la placa no tiene a nadie al mando. Con terapia en marcha
  // eso contradice a la identidad; sin ella no hay nada que pintar todavia.
  return board->therapyOn ? PATIENT_CONFIRM_CLEAR : PATIENT_CONFIRM_WAIT;
}

bool patient_list_has_baby(const PatientIdentity *id,
                           const PatientListEntry *entries, int count) {
  if (!id || !entries || id->seq == 0) return false;
  for (int i = 0; i < count; i++) {
    if (patient_same_baby(id, entries[i].seq, entries[i].name,
                          entries[i].gestWeeks)) {
      return true;
    }
  }
  return false;
}

bool patient_same_baby(const PatientIdentity *id, uint32_t seq,
                       const char *name, uint8_t gestWeeks) {
  if (!id || !name || id->seq == 0 || id->seq != seq) return false;
  return id->gestWeeks == gestWeeks &&
         strncmp(id->name, name, PATIENT_NAME_LEN) == 0;
}

bool patient_credit_allowed(uint32_t accSeq, bool sendsActiveSeq,
                            uint32_t activeSeq, bool confirmed) {
  if (accSeq == 0) return false;
  return sendsActiveSeq ? (activeSeq == accSeq) : confirmed;
}

// RED: stubs sin comportamiento.
bool patient_live_session(uint32_t, bool, bool) { return false; }
bool patient_confirm_requires_list(uint32_t, uint32_t) { return false; }
bool patient_list_request_due(bool, uint32_t, uint32_t, uint8_t, uint8_t,
                              uint32_t) {
  return false;
}

PatientDisplay patient_display_state(bool confirmed, bool listedOnLegacyBoard) {
  if (confirmed) return PATIENT_DISPLAY_CONFIRMED;
  return listedOnLegacyBoard ? PATIENT_DISPLAY_UNCONFIRMED
                             : PATIENT_DISPLAY_HIDDEN;
}

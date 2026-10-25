#include "state/patient_state.h"

#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>

#include "CommTask.h"        // ctrl_state_msg, instantanea de la placa, lista
#include "EEPROM_defines.h"  // HMI_NS_CFG, HMI_KEY_PAT_*
#include "esp_log.h"
#include "state/training_mode.h"  // Training_IsActive(), Training_IsPracticeSeq()
#include "modules/maintenance/maintenance.h"  // Maintenance_NoteSeqDropped()
#include "ui/BabyHistory.h"       // BabyHistory_IsOpen()
#include "ui/BabyWizard.h"        // SetSession(), ClearActiveProfile(), IsOpen()

namespace {

constexpr const char *TAG = "PATIENT";

constexpr uint32_t SERVICE_PERIOD_MS = 1000u;
constexpr uint32_t NVS_RETRY_MS = 5000u;
// Peticiones de PROFILE_LIST propias (placa antigua, o minutos por sembrar):
// pocas y espaciadas. No hay peticionario periodico (el flag de la lista tiene
// un unico consumidor), esto es solo la comprobacion de despues de un reinicio.
constexpr uint8_t LIST_MAX_ATTEMPTS = 3;
constexpr uint32_t LIST_RETRY_MS = 10000u;

PatientIdentity s_id = {};  // seq 0 = nadie
bool s_idDirty = false;
bool s_erasePending = false;
// Ultimo fallo de escritura de la identidad. Se guarda el INSTANTE del fallo y
// no un "reintentar a las": comparar contra un plazo absoluto con millis() se
// rompe al pasar de 24,8 dias de funcionamiento (int32 con signo).
bool s_idWriteFailed = false;
uint32_t s_idFailedAtMs = 0;

// Confirmacion con la placa. Solo tienen sentido con s_id.seq != 0.
bool s_confirmed = false;  // la placa confirma este bebe
bool s_listed = false;     // placa antigua: esta en su lista (gris con "?")
bool s_legacyBoard = false;  // la placa no manda activeSeq (se refresca CADA pasada)
// Contraste de nombre y semanas con la siguiente PROFILE_LIST: tras confirmar por
// activeSeq, porque un seq se REUTILIZA (babyStore_wipeAll reinicia la cuenta en
// 1) y el mismo numero puede ser ya otro bebe.
bool s_needsListCheck = false;
// activeSeq coincide pero el sn no es verificable (0): no se confirma solo por
// activeSeq, hace falta ademas que la lista traiga al bebe.
bool s_activeMatchedNoSn = false;
// Descarte aplazado: no se descarta con el asistente abierto (su bebe en curso
// vive en s_seq/s_name, y ClearActiveProfile() se los llevaria por delante).
const char *s_deferredDropWhy = nullptr;

PatientAccum s_acc = {};  // seq 0 = nadie
uint32_t s_mirrorGen = 0;
uint32_t s_lastServiceMs = 0;
bool s_serviceStarted = false;

// Peticiones de lista propias.
uint8_t s_listAttempts = 0;
bool s_listOutstanding = false;
uint32_t s_listAskedAtMs = 0;

// En formacion nada de esto cuenta: el bebe de practicas no es un paciente.
bool ignoredNow(uint32_t seq) {
  return Training_IsActive() || (seq != 0 && Training_IsPracticeSeq(seq));
}

uint32_t boardSnNow() {
  PatientBoardView b;
  return Communication_GetBoardView(&b) ? b.sn : 0u;
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

void resetState() {
  memset(&s_id, 0, sizeof(s_id));
  patient_accum_init(&s_acc);
  s_idDirty = false;
  s_confirmed = false;
  s_listed = false;
  s_legacyBoard = false;
  s_needsListCheck = false;
  s_activeMatchedNoSn = false;
  s_deferredDropWhy = nullptr;
  s_listAttempts = 0;
  s_listOutstanding = false;
}

// La placa contradice la identidad (o el HMI esta ante otra placa): fuera, de la
// RAM, de la NVS y del asistente, que si no seguiria creyendo en ella.
// BabyWizard_ClearActiveProfile() vuelve a llamar a OnClear, que ya no encuentra
// nada que borrar.
//
// No se descarta con el asistente abierto: su bebe en curso vive en s_seq/s_name
// y ClearActiveProfile() los pondria a cero a mitad de un alta. Se aplaza hasta
// que se cierre (o hasta que el propio asistente confirme/sustituya la identidad).
void dropIdentity(const char *why) {
  if (BabyWizard_IsOpen()) {
    s_deferredDropWhy = why;
    return;
  }
  ESP_LOGW(TAG, "identidad seq=%lu descartada: %s", (unsigned long)s_id.seq, why);
  // Descartar una identidad que la placa nunca confirmo NO es un alta del
  // paciente: sin esto Maintenance_Tick() veria seq -> 0 y dejaria una limpieza
  // terminal pendiente, y persistida.
  if (!s_confirmed) Maintenance_NoteSeqDropped(s_id.seq);
  resetState();
  s_erasePending = true;
  BabyWizard_ClearActiveProfile();
}

// Pide una PROFILE_LIST propia si procede. No compite con el asistente ni con
// el visor: con alguno abierto espera. Una peticion en vuelo CADUCA tras
// LIST_RETRY_MS (si la respuesta se pierde, antes no se reintentaba nunca).
void requestListIfNeeded(bool linkOk) {
  if (!linkOk) return;
  if (BabyWizard_IsOpen() || BabyHistory_IsOpen()) return;
  const uint32_t now = millis();
  if (!patient_list_request_due(s_listOutstanding, s_listAskedAtMs, now,
                                s_listAttempts, LIST_MAX_ATTEMPTS,
                                LIST_RETRY_MS)) {
    return;
  }
  Communication_SendProfileListReq();
  s_listAttempts++;
  s_listAskedAtMs = now;
  s_listOutstanding = true;
}

// Una lista de la placa ha llegado (la pidiera quien la pidiera).
void onListArrived(const BabyProfileListMsg &m) {
  if (s_id.seq == 0) return;

  // Contraste de nombre y semanas. Hace falta (a) con una placa antigua, donde es
  // lo unico con lo que se puede avalar la identidad, y (b) tras confirmar por
  // activeSeq, porque un seq se reutiliza (babyStore_wipeAll lo reinicia a 1) y
  // el mismo numero puede ser ya otro bebe. Una identidad nacida de un ACK acaba
  // de ser avalada por la placa y no lo necesita.
  const bool legacyCheck = s_legacyBoard && !s_confirmed && !s_listed;
  if (s_needsListCheck || legacyCheck) {
    PatientListEntry entries[3];
    const int n = m.count < 3 ? m.count : 3;
    for (int i = 0; i < n; i++) {
      entries[i].seq = m.items[i].seq;
      snprintf(entries[i].name, sizeof(entries[i].name), "%s", m.items[i].name);
      entries[i].gestWeeks = m.items[i].gestWeeks;
    }
    if (!patient_list_has_baby(&s_id, entries, n)) {
      dropIdentity("no figura en la lista de la placa (o con otro nombre)");
      return;
    }
    s_needsListCheck = false;
    if (s_activeMatchedNoSn) {
      // activeSeq coincidia y la lista lo avala: ahora si, confirmado.
      s_activeMatchedNoSn = false;
      s_confirmed = true;
      s_listed = false;
    } else if (!s_confirmed) {
      s_listed = true;  // placa antigua: gris con "?", nunca como confirmada
    }
  }

  // Minutos: solo de un bebe que la placa avala (confirmado o en su lista).
  if (!(s_confirmed || s_listed) || s_acc.seq == 0) return;
  for (int i = 0; i < m.count && i < 3; i++) {
    if (m.items[i].seq != s_acc.seq) continue;
    const PatientMinutes board = {m.items[i].phototherapyMinutes,
                                  m.items[i].thermoMinutes,
                                  m.items[i].humidityMinutes};
    patient_accum_resync(&s_acc, s_acc.seq, &board);
    break;
  }
}

void serviceMirror() {
  BabyProfileListMsg m;
  if (!Communication_TakeProfileListMirror(&s_mirrorGen, &m)) return;
  // Si la lista era la respuesta a una peticion MIA y nadie mas esta esperando
  // una, se baja el flag del unico consumidor: sin esto una lista vieja se
  // tomaria por la respuesta fresca de la siguiente peticion del asistente.
  if (s_listOutstanding && !BabyWizard_IsOpen() && !BabyHistory_IsOpen()) {
    g_pendingProfileList = false;
  }
  s_listOutstanding = false;
  onListArrived(m);
}

// Confirma (o descarta) una identidad restaurada con lo que dice la placa.
void serviceConfirmation(bool linkOk) {
  if (s_id.seq == 0) return;
  if (s_deferredDropWhy) {  // descarte que esperaba a que se cerrase el asistente
    dropIdentity(s_deferredDropWhy);
    return;
  }
  // Con el enlace caido el ultimo CTRL,STATE esta obsoleto: no dice nada de
  // quien esta ahora al mando, y decidir con el podria borrar un bebe valido.
  if (!linkOk) return;
  PatientBoardView b;
  if (!Communication_GetBoardView(&b)) return;  // la placa aun no ha hablado
  s_legacyBoard = !b.sendsActiveSeq;            // se refresca en CADA pasada
  if (s_confirmed) return;

  switch (patient_confirm_decide(s_id.seq, s_id.boardSn, &b)) {
    case PATIENT_CONFIRM_CONFIRM:
      s_listed = false;
      s_needsListCheck = true;  // seq reutilizable: se contrasta con la lista
      if (patient_confirm_requires_list(s_id.boardSn, b.sn)) {
        // Sin numero de serie que verificar, activeSeq por si solo no basta.
        s_activeMatchedNoSn = true;
        ESP_LOGI(TAG, "identidad seq=%lu: activeSeq coincide, sn sin verificar:"
                      " espera a la lista", (unsigned long)s_id.seq);
      } else {
        s_confirmed = true;
        ESP_LOGI(TAG, "identidad seq=%lu confirmada por activeSeq",
                 (unsigned long)s_id.seq);
      }
      break;
    case PATIENT_CONFIRM_CLEAR:
      dropIdentity((s_id.boardSn != 0 && b.sn != 0 && b.sn != s_id.boardSn)
                       ? "es de otra placa"
                       : "la placa tiene otro bebe al mando");
      return;
    case PATIENT_CONFIRM_WAIT:
      break;  // placa antigua: la lista decide (Service pide la lista)
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
    if (idHave != 0 || thHave != 0) {
      // Version 1 (40 B, sin sn), corrupto o de otro tamano: no se puede atar a
      // ninguna placa. Se descarta y se borra para no reintentarlo cada arranque.
      ESP_LOGW(TAG, "pat_id invalido o de version antigua (%u B): descartado",
               (unsigned)idHave);
      s_erasePending = true;
    }
    return;
  }

  // Primero el estado propio, luego el asistente: asi el gancho de SetSession
  // ve el mismo bebe y no lo toma por uno nuevo. Nace SIN CONFIRMAR.
  s_id = id;
  s_confirmed = false;
  patient_accum_begin(&s_acc, id.seq, nullptr);  // minutos desconocidos...
  PatientTherapyRecord rec;
  bool therapyRestored = false;
  if (thGot == sizeof(thBlob) &&
      patient_therapy_decode(thBlob, sizeof(thBlob), &rec)) {
    // ...salvo que haya un registro de ESTE bebe: dos blobs, dos escrituras, y
    // un corte entre ambas puede dejar acumulados de otro.
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
  ESP_LOGI(TAG,
           "restaurado SIN CONFIRMAR seq=%lu gest=%u age=%d ther=%d "
           "(%lu/%lu/%lu min)",
           (unsigned long)id.seq, (unsigned)id.gestWeeks, (int)id.ageKnown,
           (int)therapyRestored, (unsigned long)m.photo, (unsigned long)m.thermo,
           (unsigned long)m.humidity);
}

void PatientState_OnSessionSet(uint32_t seq, const char *name,
                               uint8_t gestWeeks, const PatientMinutes *seed,
                               bool fromBoardAck) {
  if (seq == 0) {
    PatientState_OnClear();
    return;
  }
  if (ignoredNow(seq)) return;
  const char *n = name ? name : "";

  if (patient_same_baby(&s_id, seq, n, gestWeeks)) {
    // El mismo bebe: edad y acumulados siguen valiendo (el contador local es
    // mas fresco que cualquier lista vieja).
    if (fromBoardAck) {
      s_confirmed = true;
      s_listed = false;
      // La placa acaba de responder por este bebe: nada que contrastar, y un
      // descarte aplazado (asistente abierto) ya no tiene objeto.
      s_needsListCheck = false;
      s_activeMatchedNoSn = false;
      s_deferredDropWhy = nullptr;
      const uint32_t sn = boardSnNow();
      if (s_id.boardSn != sn) {
        s_id.boardSn = sn;
        s_idDirty = true;
      }
    }
    if (s_acc.seq != seq || (!s_acc.known && seed)) {
      patient_accum_begin(&s_acc, seq, seed);
    }
    return;
  }

  // Otro bebe. Incluye el MISMO seq con otro nombre o semanas: la placa
  // reasigno el seq, y la edad y los acumulados del anterior no son de este.
  resetState();
  s_id.seq = seq;
  snprintf(s_id.name, sizeof(s_id.name), "%s", n);
  s_id.gestWeeks = gestWeeks;
  s_id.boardSn = boardSnNow();
  s_confirmed = fromBoardAck;
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
  resetState();
  s_erasePending = true;
}

PatientDisplay PatientState_GetDisplay(uint32_t seq) {
  if (seq == 0 || s_id.seq != seq) return PATIENT_DISPLAY_CONFIRMED;
  return patient_display_state(s_confirmed, s_listed);
}

bool PatientState_IsConfirmed(uint32_t seq) {
  return PatientState_GetDisplay(seq) == PATIENT_DISPLAY_CONFIRMED;
}

PatientAge PatientState_GetAge(uint32_t seq, uint8_t gestWeeks) {
  if (seq == 0 || s_id.seq != seq) return PatientAge{false, 0, false, 0, 0};
  return patient_age_compute(gestWeeks, s_id.ageKnown, s_id.ageDays,
                             s_id.captureEpoch, HMI_GetEpochNow());
}

bool PatientState_GetTherapy(uint32_t seq, PatientMinutes *out) {
  if (!out || seq == 0 || s_acc.seq != seq || !s_acc.known) return false;
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

  // El resto es el reloj de terapia y la confirmacion: ~1 Hz, no en cada pasada
  // de UI_Task.
  const uint32_t now = millis();
  if (s_serviceStarted && (now - s_lastServiceMs) < SERVICE_PERIOD_MS) return;
  s_serviceStarted = true;
  s_lastServiceMs = now;

  // Formacion o enlace caido: ctrl_state_msg esta virtualizado u obsoleto y no
  // dice nada de lo que la incubadora esta haciendo AHORA.
  const bool linkOk = !Training_IsActive() && !Display_IsBoardLinkLost();

  if (!Training_IsActive()) serviceConfirmation(linkOk);
  // Hace falta una lista (una peticion propia, no un peticionario periodico):
  // para contrastar nombre y semanas (placa antigua, o tras confirmar por
  // activeSeq), o para fijar unos minutos que no se conocen (no habia pat_ther).
  if (s_id.seq != 0) {
    const bool legacyCheck = s_legacyBoard && !s_confirmed && !s_listed;
    const bool minutesUnknown = (s_confirmed || s_listed) && !s_acc.known;
    if (s_needsListCheck || legacyCheck || minutesUnknown) {
      requestListIfNeeded(linkOk);
    }
  }
  serviceMirror();
  if (s_id.seq == 0) return;  // el acumulador sigue la identidad

  // Acreditar solo si la placa dice que ESE bebe esta al mando; sin dato de la
  // placa, solo con la identidad confirmada.
  PatientBoardView b;
  const bool haveBoard = Communication_GetBoardView(&b);
  const bool creditOk = patient_credit_allowed(
      s_acc.seq, haveBoard && b.sendsActiveSeq, haveBoard ? b.activeSeq : 0u,
      s_confirmed);
  patient_accum_tick(&s_acc, now, (uint8_t)ctrl_state_msg.actuation,
                     ctrl_state_msg.phototherapyMode != 0, linkOk && creditOk);

  // Un fallo de escritura deja `dirty` y, como flush_done() ya anoto la hora,
  // el siguiente intento espera un intervalo entero: no hay bucle de reintentos.
  if (patient_accum_flush_due(&s_acc, now)) writeTherapy(now);
}

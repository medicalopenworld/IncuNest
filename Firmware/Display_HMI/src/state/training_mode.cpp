#include "training_mode.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "nte_table.h"

static const char *TAG = "Training";

namespace {

// Retardo de las respuestas simuladas. La placa real tarda del orden de una
// vuelta de su bucle; un retardo visible evita que un asistente consuma la
// respuesta en la misma pasada en que la pidio y hace que la pantalla de
// "cargando" se vea, como en real.
constexpr uint32_t SIM_REPLY_DELAY_MS = 250;

enum SimKind {
  SIM_NONE = 0,
  SIM_LIST,
  SIM_ACK,
  SIM_RANGE,
  SIM_WEIGHT_HIST,
  SIM_TIME_ACK
};

volatile bool s_active = false;
bool s_exitDialogAllowed = false;
HMI_Message s_frozen;
uint32_t s_restoreUntilMs = 0;
volatile bool s_forceSend = false;

// Bebe de formacion: lo que el asistente ha ido contando, para calcular el
// rango NTE local igual que haria la placa.
uint8_t s_gestWeeks = 0;
uint16_t s_weightGrams = 0;
uint16_t s_ageDays = 0;
bool s_ageKnown = false;

// A quien se refiere el flujo en curso (ZOE o el registrado en la leccion):
// es el seq que llevan el ACK y el rango simulados.
uint32_t s_curSeq = TRAINING_BABY_SEQ;

// Bebe registrado por el alumno desde Bebes durante la leccion
// (TRAINING_NEW_BABY_SEQ). Se lista junto a ZOE hasta que la leccion acaba.
bool s_newUsed = false;
char s_newName[24] = "";
uint8_t s_newGest = 0;
uint16_t s_newWeight = 0;

// seq cuya curva de peso se ha pedido (SIM_WEIGHT_HIST).
uint32_t s_histSeq = 0;

SimKind s_simKind = SIM_NONE;
uint32_t s_simDueMs = 0;

void schedule(SimKind kind) {
  s_simKind = kind;
  s_simDueMs = millis() + SIM_REPLY_DELAY_MS;
}

}  // namespace

void Training_Enter(void) {
  if (s_active) return;
  // Copia ANTES de levantar el flag: CommTask (otro core) lee la copia solo
  // cuando ve el flag. La barrera impide que compilador o CPU reordenen la
  // copia por detras del flag.
  memcpy(&s_frozen, &hmi_msg, sizeof(s_frozen));
  s_frozen.shouldSendData = false;
  s_gestWeeks = 0;
  s_weightGrams = 0;
  s_ageDays = 0;
  s_ageKnown = false;
  s_curSeq = TRAINING_BABY_SEQ;
  s_newUsed = false;
  s_newName[0] = '\0';
  s_newGest = 0;
  s_newWeight = 0;
  s_simKind = SIM_NONE;
  s_exitDialogAllowed = false;
  __sync_synchronize();
  s_active = true;
  ESP_LOGW(TAG, "MODO FORMACION activado: la placa no recibe ordenes");
}

void Training_SetExitDialogAllowed(bool allowed) { s_exitDialogAllowed = allowed; }
bool Training_ExitDialogAllowed(void) { return s_exitDialogAllowed; }

void Training_Exit(void) {
  if (!s_active) return;
  // La restauracion de hmi_msg vive AQUI, junto al flag, y no en el llamador:
  // asi ningun camino de salida puede dejar encendido lo que el alumno
  // encendio. Con shouldSendData la placa recibe el estado previo en la
  // siguiente vuelta de CommTask (<= 10 ms), no en el siguiente keepalive.
  memcpy(&hmi_msg, &s_frozen, sizeof(hmi_msg));
  hmi_msg.shouldSendData = false;
  s_restoreUntilMs = millis() + TRAINING_RESTORE_GUARD_MS;
  __sync_synchronize();
  s_active = false;
  s_forceSend = true;  // CommTask lo consume y envia en su siguiente vuelta
  s_simKind = SIM_NONE;
  s_exitDialogAllowed = false;
  ESP_LOGW(TAG, "MODO FORMACION desactivado: estado previo reenviado a la placa");
}

bool Training_RestoreGuardActive(void) {
  return s_restoreUntilMs != 0 &&
         (int32_t)(millis() - s_restoreUntilMs) < 0;
}

bool Training_TakeForceSend(void) {
  if (!s_forceSend) return false;
  s_forceSend = false;
  return true;
}

bool Training_IsActive(void) { return s_active; }

bool Training_IsPracticeSeq(uint32_t seq) {
  return seq == TRAINING_BABY_SEQ || seq == TRAINING_NEW_BABY_SEQ;
}

const HMI_Message &Training_FrozenHmiMsg(void) { return s_frozen; }

// ---- Respuestas simuladas -------------------------------------------------
// Espejo de lo que pone parse_message() en CommTask.cpp para CTRL,PROFILE_LIST,
// CTRL,PROFILE_ACK, CTRL,PROFILE_RANGE y CTRL,TIME_ACK. Si cambia el formato de
// alguna de esas respuestas, cambiar aqui tambien.

void Training_SimProfileListReq(void) {
  // Lista con ZOE y, si el alumno registro uno desde Bebes en esta leccion,
  // ese segundo bebe de practica. Asi el alumno practica la seleccion de un
  // bebe existente y nunca ve ni toca uno real.
  schedule(SIM_LIST);
}

void Training_SimProfileSelect(uint32_t seq) {
  if (seq == TRAINING_NEW_BABY_SEQ && s_newUsed) {
    s_curSeq = TRAINING_NEW_BABY_SEQ;
    s_gestWeeks = s_newGest;
    s_weightGrams = s_newWeight;
  } else {
    // Cualquier otro seq (ZOE, o uno real que no deberia llegar aqui) se
    // contesta como ZOE: la placa real contestaria 0 a un seq desconocido,
    // pero en formacion no hay nada que rechazar.
    s_curSeq = TRAINING_BABY_SEQ;
    s_gestWeeks = TRAINING_BABY_GEST_WEEKS;
    s_weightGrams = TRAINING_BABY_WEIGHT_G;
  }
  s_ageDays = 0;
  s_ageKnown = false;
  schedule(SIM_ACK);
}

void Training_SimProfileNew(const char *name, uint8_t gestWeeks) {
  // Registro desde Bebes durante la leccion: nace el segundo bebe de practica
  // con lo que tecleo el alumno. Un registro posterior lo sustituye.
  s_newUsed = true;
  snprintf(s_newName, sizeof(s_newName), "%s", name ? name : "");
  s_newGest = gestWeeks;
  s_newWeight = 0;
  s_curSeq = TRAINING_NEW_BABY_SEQ;
  s_gestWeeks = gestWeeks;
  s_weightGrams = 0;
  s_ageDays = 0;
  s_ageKnown = false;
  schedule(SIM_ACK);
}

void Training_SimProfileWeight(uint32_t seq, uint16_t grams) {
  s_weightGrams = grams;
  // El peso del bebe registrado se refleja en su fila de la lista, como haria
  // la placa; el de ZOE es fijo (1500 g) a proposito.
  if (seq == TRAINING_NEW_BABY_SEQ && s_newUsed && grams > 0) {
    s_newWeight = grams;
  }
  // Un bebe nuevo no tiene edad conocida: la placa contesta ageKnown=0 y el
  // asistente pide los dias de vida, igual que en real.
  s_ageKnown = false;
  schedule(SIM_RANGE);
}

void Training_SimProfileAgeManual(uint32_t seq, uint16_t ageDays) {
  (void)seq;
  s_ageDays = ageDays;
  s_ageKnown = true;
  schedule(SIM_RANGE);
}

void Training_SimWeightHistoryReq(uint32_t seq) {
  s_histSeq = seq;
  schedule(SIM_WEIGHT_HIST);
}

void Training_SimSetTime(void) { schedule(SIM_TIME_ACK); }

void Training_ServiceReplies(void) {
  if (!s_active || s_simKind == SIM_NONE) return;
  if ((int32_t)(millis() - s_simDueMs) < 0) return;
  const SimKind kind = s_simKind;
  s_simKind = SIM_NONE;

  switch (kind) {
    case SIM_LIST: {
      BabyProfileListItem &z = g_profileList.items[0];
      memset(&z, 0, sizeof(z));
      z.seq = TRAINING_BABY_SEQ;
      strncpy(z.name, TRAINING_BABY_NAME, sizeof(z.name) - 1);
      z.gestWeeks = TRAINING_BABY_GEST_WEEKS;
      z.weightGrams = TRAINING_BABY_WEIGHT_G;
      g_profileList.count = 1;
      if (s_newUsed) {
        BabyProfileListItem &n = g_profileList.items[1];
        memset(&n, 0, sizeof(n));
        n.seq = TRAINING_NEW_BABY_SEQ;
        strncpy(n.name, s_newName, sizeof(n.name) - 1);
        n.gestWeeks = s_newGest;
        n.weightGrams = s_newWeight;
        g_profileList.count = 2;
      }
      g_pendingProfileList = true;
      break;
    }
    case SIM_ACK:
      g_profileAck = s_curSeq;
      g_pendingProfileAck = true;
      break;
    case SIM_RANGE: {
      // Misma funcion pura que usa la placa (shared/include/nte_table.h): el
      // alumno ve el mismo rango que veria en real con esos datos.
      const NteRange r = calculateNteRange(s_weightGrams, s_gestWeeks,
                                          s_ageKnown ? s_ageDays : 0);
      g_profileRange.seq = s_curSeq;
      g_profileRange.ageKnown = s_ageKnown;
      g_profileRange.ageDays = s_ageDays;
      g_profileRange.lo = r.lo;
      g_profileRange.hi = r.hi;
      g_profileRange.mid = r.mid;
      g_profileRange.estimated = r.estimated;
      g_pendingProfileRange = true;
      break;
    }
    case SIM_WEIGHT_HIST: {
      // Un solo punto, el peso que muestra la lista (dia 0), o ninguno si no
      // lo tiene: lo mismo que la placa devolveria para un bebe recien
      // registrado con o sin peso.
      uint16_t w = 0;
      if (s_histSeq == TRAINING_BABY_SEQ) {
        w = TRAINING_BABY_WEIGHT_G;
      } else if (s_histSeq == TRAINING_NEW_BABY_SEQ && s_newUsed) {
        w = s_newWeight;
      }
      g_weightHistory.seq = s_histSeq;
      g_weightHistory.count = 0;
      if (w > 0) {
        g_weightHistory.dayOffset[0] = 0;
        g_weightHistory.weightGrams[0] = w;
        g_weightHistory.count = 1;
      }
      g_pendingWeightHistory = true;
      break;
    }
    case SIM_TIME_ACK:
      g_timeAckResult = 0;
      g_pendingTimeAck = true;
      break;
    default:
      break;
  }
}

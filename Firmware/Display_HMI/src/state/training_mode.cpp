#include "training_mode.h"

#include <Arduino.h>

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

enum SimKind { SIM_NONE = 0, SIM_LIST, SIM_ACK, SIM_RANGE, SIM_TIME_ACK };

volatile bool s_active = false;
bool s_exitDialogAllowed = false;
HMI_Message s_frozen;

// Bebe de formacion: lo que el asistente ha ido contando, para calcular el
// rango NTE local igual que haria la placa.
uint8_t s_gestWeeks = 0;
uint16_t s_weightGrams = 0;
uint16_t s_ageDays = 0;
bool s_ageKnown = false;

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
  hmi_msg.shouldSendData = true;
  __sync_synchronize();
  s_active = false;
  s_simKind = SIM_NONE;
  s_exitDialogAllowed = false;
  ESP_LOGW(TAG, "MODO FORMACION desactivado");
}

bool Training_IsActive(void) { return s_active; }

const HMI_Message &Training_FrozenHmiMsg(void) { return s_frozen; }

// ---- Respuestas simuladas -------------------------------------------------
// Espejo de lo que pone parse_message() en CommTask.cpp para CTRL,PROFILE_LIST,
// CTRL,PROFILE_ACK, CTRL,PROFILE_RANGE y CTRL,TIME_ACK. Si cambia el formato de
// alguna de esas respuestas, cambiar aqui tambien.

void Training_SimProfileListReq(void) {
  // Lista con un unico bebe: ZOE. Asi el alumno practica la seleccion de un
  // bebe existente y nunca ve ni toca uno real.
  schedule(SIM_LIST);
}

void Training_SimProfileSelect(uint32_t seq) {
  (void)seq;
  s_gestWeeks = TRAINING_BABY_GEST_WEEKS;
  s_weightGrams = TRAINING_BABY_WEIGHT_G;
  s_ageDays = 0;
  s_ageKnown = false;
  schedule(SIM_ACK);
}

void Training_SimProfileNew(const char *name, uint8_t gestWeeks) {
  (void)name;
  s_gestWeeks = gestWeeks;
  s_weightGrams = 0;
  s_ageDays = 0;
  s_ageKnown = false;
  schedule(SIM_ACK);
}

void Training_SimProfileWeight(uint32_t seq, uint16_t grams) {
  (void)seq;
  s_weightGrams = grams;
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
      g_pendingProfileList = true;
      break;
    }
    case SIM_ACK:
      g_profileAck = TRAINING_BABY_SEQ;
      g_pendingProfileAck = true;
      break;
    case SIM_RANGE: {
      // Misma funcion pura que usa la placa (shared/include/nte_table.h): el
      // alumno ve el mismo rango que veria en real con esos datos.
      const NteRange r = calculateNteRange(s_weightGrams, s_gestWeeks,
                                          s_ageKnown ? s_ageDays : 0);
      g_profileRange.seq = TRAINING_BABY_SEQ;
      g_profileRange.ageKnown = s_ageKnown;
      g_profileRange.ageDays = s_ageDays;
      g_profileRange.lo = r.lo;
      g_profileRange.hi = r.hi;
      g_profileRange.mid = r.mid;
      g_profileRange.estimated = r.estimated;
      g_pendingProfileRange = true;
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

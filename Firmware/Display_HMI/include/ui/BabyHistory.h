#pragma once
// "Babies" history screen (baby-history-viewer capability): active profiles
// with a Discharge action and a NEW BABY registration flow, archived profiles
// paginated 10/page, and a weight-evolution chart per baby. Fully independent
// of the AIR/SKIN activation wizard — works with no control active.
#include <lvgl.h>
#include <stdint.h>

// Creates the (hidden) overlay. Call once at UI init, after the main screen.
void BabyHistory_Init(lv_obj_t *parent);

// Opens the screen: requests the active list + first archived page.
void BabyHistory_Open(void);

// Drives timeouts/response handling. Call from the UI task loop (LVGL locked),
// same contract as BabyWizard_Poll().
void BabyHistory_Poll(void);

// Para el motor de lecciones (hmi-training-courses).
bool BabyHistory_IsOpen(void);
void BabyHistory_Close(void);

// Fase de la pantalla, agrupada como BabyWizard_GetStep(): lo que una leccion
// necesita para saber por donde va el alumno sin exponer el enum interno.
typedef enum {
  BH_CLOSED = 0,
  BH_LIST,         // cargando o mostrando la lista (incluye el dialogo de alta)
  BH_NEW_NAME,     // registro: nombre
  BH_NEW_GEST,     // registro: semanas de gestacion
  BH_NEW_WEIGHT,   // registro: peso al ingreso (o SIN PESO) y REGISTRAR
  BH_NEW_WAITING,  // registro enviado, esperando a la placa
  BH_CHART,        // curva de peso de un bebe
} BabyHistoryStep;
BabyHistoryStep BabyHistory_GetStep(void);

// seq del ultimo bebe registrado desde esta pantalla (0 si ninguno desde que
// se abrio). Estado "ya registro uno" para el objetivo de la leccion.
uint32_t BabyHistory_LastRegisteredSeq(void);

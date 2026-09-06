#pragma once
// Asks what happened to the baby when the incubator goes fully idle.
//
// Deliberately NOT triggered by turning off one control among several: with
// temperature and phototherapy both running, dropping one only changes the
// therapy, the baby is still inside. Only the transition to "everything off"
// means the baby actually came out, which is the moment worth recording.
//
// Two outcomes, which are very different in the data model:
//   - with the mother (kangaroo): profile stays active, event counted
//   - discharge: profile is archived with its clinical outcome
#include <lvgl.h>

void BabyExitDialog_Init(lv_obj_t *parent);

// True mientras alguna de las preguntas este en pantalla. Lo consultan los
// overlays que esperan turno (MaintenanceDialog) para no pintarse encima.
bool BabyExitDialog_IsOpen(void);

// Call once per UI_Task tick, inside LVGL_Lock(), with the current control
// state. Handles the idle-edge detection itself and opens the dialog when
// warranted; cheap no-op otherwise.
void BabyExitDialog_Tick(bool anyControlActive);

// Para el motor de lecciones (hmi-training-courses): saber si esta abierto y
// cerrarlo sin registrar nada (como el boton X).
bool BabyExitDialog_IsOpen(void);
void BabyExitDialog_Cancel(void);

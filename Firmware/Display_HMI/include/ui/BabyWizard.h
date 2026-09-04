#pragma once
// Mandatory baby-data activation wizard (temp-control-activation-wizard
// spec). Replaces the former AUTO AIR popup: triggered on every OFF->ON
// transition of the temperature switch, drives the PROFILE_* protocol
// (Firmware/PROTOCOL.md v1.6.0), and hands off to ActivateTempControlUI()
// (UITask.cpp) on Apply.
#include <lvgl.h>

// Creates the overlay/card widgets (hidden). Call once during UI init.
void BabyWizard_Init(lv_obj_t *parent);

// Opens the wizard for the given desired mode (true=AIR, false=SKIN,
// matching the CONTROL_AIR/CONTROL_SKIN macros in main.h). Called from
// Switch_cb's ui_Switch1 ON branch instead of activating control directly.
void BabyWizard_Open(bool desiredIsAirMode);

// Same wizard, but gating phototherapy instead of a thermal control mode.
// No NTE range applies to a lamp, so the summary shows no temperature
// proposal and Apply hands off to ActivatePhototherapyFromWizard() — the
// baby data itself (name, gestational age, weight) is still worth
// collecting, which is the whole point of running the wizard here too.
void BabyWizard_OpenForPhototherapy(void);

// Same wizard, gating humidity instead of a thermal control mode. A
// humidifier has no NTE range either, so this collects the baby data and
// hands off to ActivateHumidityFromWizard() — humidity is a therapy applied
// to a specific baby, and its hours are accounted per profile, so it must be
// clear who is receiving it.
void BabyWizard_OpenForHumidity(void);

// Consumes CommTask's pending PROFILE_* responses, screen timeouts, and the
// critical-alarm-interrupts-wizard rule. Call once per UI_Task loop tick,
// inside LVGL_Lock().
void BabyWizard_Poll(void);

// True once the current activation session has a usable (non-SKIP,
// non-estimated) NTE range, i.e. whether the proposed AIR temperature this
// session is running with came from a real weight or from an estimate.
// Deliberately NOT a gate on anything: SKIN mode used to require it, and no
// longer does (only a connected probe does). Resets to false at the start of
// every BabyWizard_Open().
bool BabyWizard_HasUsableRange(void);

// The profile this HMI last put in charge of the incubator (0 = none/SKIP).
// Used by the exit dialog to know which baby just came out. Name is "" when
// unknown.
uint32_t BabyWizard_GetActiveSeq(void);
const char *BabyWizard_GetActiveName(void);

// True when this HMI knows which baby is in the incubator AND some therapy is
// running for them right now (UI_AnyControlActive()) — i.e. the identity is
// current, not just remembered. This is the only condition under which a
// therapy may reuse that identity instead of re-running the baby picker:
// adding phototherapy to a running temperature control does not re-ask, but
// activating anything after the incubator went fully idle does, because the
// care session ended there. Deliberately independent of what the exit dialog
// answered (kangaroo / "not now" / never shown): that answer belongs to the
// clinical record, not to who the picker offers.
bool BabyWizard_HasLiveSession(void);
// Cleared when a baby is discharged so the exit dialog stops offering it.
void BabyWizard_ClearActiveProfile(void);

// --- Para el motor de lecciones (hmi-training-courses) -------------------
// Fase del asistente, agrupada: lo que una leccion necesita para saber "por
// donde va" el alumno sin exponer el enum interno.
typedef enum {
  BW_CLOSED = 0,
  BW_PICKER,    // cargando lista / elegir bebe
  BW_IDENTITY,  // nombre o semanas de gestacion
  BW_WEIGHT,    // peso (o esperando el rango)
  BW_AGE,       // dias de vida (o esperando el rango)
  BW_SUMMARY,   // resumen, APLICAR / SALTAR
} BabyWizardStep;
BabyWizardStep BabyWizard_GetStep(void);
bool BabyWizard_IsOpen(void);
// Cierra el asistente como si el operador hubiera pulsado SALIR: revierte el
// switch que lo abrio y deja el estado Closed. No toca el perfil activo.
void BabyWizard_Cancel(void);

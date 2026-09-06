#pragma once
// Modo formacion (spec hmi-training-courses, ADR-0002 revisado 2026-09-05).
//
// Mientras esta activo, la HMI se comporta con normalidad y la ACTUACION es
// real: la lampara de fototerapia y el calefactor se encienden de verdad
// (con la incubadora vacia: el gate clinico exige que no haya terapia ni bebe
// real). Lo que se virtualiza es el bebe y todo lo que se REGISTRA:
//   - El asistente ve un unico bebe de practica, ZOE (TRAINING_BABY_SEQ), y
//     obliga a elegirla (BEBE NUEVO y SALTAR se rechazan). Sus peticiones de
//     perfil (lista, seleccion, peso, edad) y la hora se contestan en local
//     con los mismos flags g_pending* que pondria el parser; alta, salida,
//     canguro y credenciales WiFi se tragan. ZOE nunca llega a la placa ni al
//     historial ni a ThingsBoard.
//   - Nada cambiado en formacion se persiste en NVS.
//   - Al salir, Training_Exit() restaura hmi_msg desde la instantanea tomada
//     al entrar y fuerza un envio: la placa vuelve al estado previo (todo
//     apagado si asi estaba).
//
// Es el interruptor unico del modo: si algun dia la motherBoard debe saber que
// hay formacion, se anade desde aqui.
#include <stdbool.h>
#include <stdint.h>

#include "CommTask.h"

// Bebe de practica. seq fuera del rango real (la placa numera desde 1); se
// limpia al salir con BabyWizard_ClearActiveProfile().
#define TRAINING_BABY_SEQ 0xFFFFu
#define TRAINING_BABY_NAME "ZOE"
#define TRAINING_BABY_GEST_WEEKS 32
#define TRAINING_BABY_WEIGHT_G 1500

// Watchdog de la lampara en formacion: si la HMI se reinicia o se cuelga con
// la fototerapia encendida, la placa mantiene la terapia (ALARM_HMI_LINK_LOST
// no la corta) y solo la apaga el temporizador. En formacion la trama de
// estado nunca sale con la lampara ON y 0 minutos: se sustituye por este tope.
#define TRAINING_PHOTO_TIMER_MIN 5

// Tras Exit(), CTRL,STATE de la placa puede llegar todavia con el estado de la
// leccion (esta en vuelo) y sobrescribir las consignas restauradas, que no
// tienen gracia de eco en CommTask. Durante esta ventana Display_ApplyCtrlState
// solo aplica identidad y alarmas.
#define TRAINING_RESTORE_GUARD_MS 2500u
bool Training_RestoreGuardActive(void);

// CommTask (su propia tarea): verdadero una sola vez tras Exit(), para forzar
// el envio del estado restaurado sin tocar hmi_msg.shouldSendData desde la UI
// (carrera con el envio en curso).
bool Training_TakeForceSend(void);

// Entra/sale. Solo desde la tarea UI, bajo LVGL_Lock(). Enter toma la
// instantanea de hmi_msg; Exit la RESTAURA en hmi_msg con shouldSendData para
// que la placa reciba de inmediato el estado previo (el invariante lo
// garantiza este modulo, no el llamador). El estado de la UI (consignas,
// switches, idioma) lo restaura el llamador con UI_RestoreControlSnapshot()
// ANTES de llamar a Exit.
void Training_Enter(void);
void Training_Exit(void);
bool Training_IsActive(void);

// Instantanea de hmi_msg tomada al entrar (lo que la placa tenia).
const HMI_Message &Training_FrozenHmiMsg(void);

// El dialogo de salida del bebe (BabyExitDialog) no se abre en formacion
// salvo que la leccion lo pida expresamente (leccion de salida del bebe).
// Se resetea a falso en Enter/Exit.
void Training_SetExitDialogAllowed(bool allowed);
bool Training_ExitDialogAllowed(void);

// ---- Respuestas simuladas (las llama CommTask en lugar de enviar) ---------
// Cada una programa una respuesta local con un pequeno retardo (como la placa
// real) que Training_ServiceReplies() entrega poniendo los g_pending*.
void Training_SimProfileListReq(void);
void Training_SimProfileSelect(uint32_t seq);
void Training_SimProfileNew(const char *name, uint8_t gestWeeks);
void Training_SimProfileWeight(uint32_t seq, uint16_t grams);
void Training_SimProfileAgeManual(uint32_t seq, uint16_t ageDays);
void Training_SimSetTime(void);

// Entrega las respuestas simuladas vencidas. Llamar una vez por vuelta de
// UI_Task, dentro de LVGL_Lock(), antes de los _Poll() de los asistentes.
void Training_ServiceReplies(void);

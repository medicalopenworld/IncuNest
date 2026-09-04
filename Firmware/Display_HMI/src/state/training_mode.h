#pragma once
// Modo formacion (spec hmi-training-courses, ADR-0002).
//
// Mientras esta activo, la HMI se comporta con normalidad pero la incubadora
// no se entera: CommTask sigue mandando el keepalive con la instantanea de
// hmi_msg tomada al entrar, no envia ningun cambio de estado ni peticion que
// modifique algo en la placa, y no aplica el CTRL,STATE recibido a la UI. Los
// asistentes que esperan respuesta de la placa la reciben simulada aqui, con
// los mismos flags g_pending* que pondria el parser.
//
// Es el interruptor unico del sandbox: si algun dia la motherBoard debe saber
// que hay formacion (opcion B del ADR), se anade desde aqui.
#include <stdbool.h>
#include <stdint.h>

#include "CommTask.h"

// seq del bebe de formacion en las respuestas simuladas. No coincide con
// ningun seq real (la placa numera desde 1) y se limpia al salir.
#define TRAINING_BABY_SEQ 0xFFFFu

// Entra/sale. Solo desde la tarea UI, bajo LVGL_Lock(). Enter toma la
// instantanea de hmi_msg; Exit RESTAURA hmi_msg desde esa instantanea (el
// invariante "al salir la placa recibe lo que tenia" lo garantiza este modulo,
// no el llamador) y descarta las respuestas simuladas pendientes. El estado de
// la UI (consignas, switches, idioma) lo restaura el llamador con
// UI_RestoreControlSnapshot() ANTES de llamar a Exit. La gracia de eco de
// CommTask no se toca: al detectar la restauracion protege 2,5 s unos valores
// que son exactamente los que tiene la placa.
void Training_Enter(void);
void Training_Exit(void);
bool Training_IsActive(void);

// Lo que CommTask manda como keepalive mientras dura la formacion.
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
void Training_SimProfileNew(const char *name, uint8_t gestWeeks);
void Training_SimProfileWeight(uint32_t seq, uint16_t grams);
void Training_SimProfileAgeManual(uint32_t seq, uint16_t ageDays);
void Training_SimSetTime(void);

// Entrega las respuestas simuladas vencidas. Llamar una vez por vuelta de
// UI_Task, dentro de LVGL_Lock(), antes de los _Poll() de los asistentes.
void Training_ServiceReplies(void);

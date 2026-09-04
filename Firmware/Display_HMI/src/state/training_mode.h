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
// instantanea de hmi_msg; Exit descarta las respuestas simuladas pendientes,
// limpia el perfil de formacion y anula la gracia de eco de CommTask para
// que el siguiente CTRL,STATE vuelva a mandar.
void Training_Enter(void);
void Training_Exit(void);
bool Training_IsActive(void);

// Lo que CommTask manda como keepalive mientras dura la formacion.
const HMI_Message &Training_FrozenHmiMsg(void);

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

#pragma once
// Estado y persistencia del recordatorio de pesada diaria. La decision de
// CUANDO toca avisar es `weigh_reminder.h` (pura, con tests de host); aqui
// vive lo que no se puede probar en host: la NVS, el bebe al mando y la hora.
//
// El protocolo no da el epoch de la ultima pesada (`CTRL,WEIGHT_HISTORY` solo
// trae el dia relativo a la admision), asi que el HMI lo sella el mismo al
// ENVIAR una pesada real. Un sello va siempre con el `seq` al que pertenece.
//
// Las escrituras NVS no se hacen en el momento: se marca "sucio" y
// `WeighTracker_Flush()` escribe FUERA de LVGL_Lock (UITask.cpp).
#include <stdbool.h>
#include <stdint.h>

// Carga el estado de NVS. Una vez en el arranque de UI_Task.
void WeighTracker_Init(void);

// Una vez por vuelta de UI_Task. Sigue al bebe al mando (alta, cambio de
// bebe), anota cuando se le vio por primera vez, sella las pesadas que no
// tenian hora y reancla fechas en el futuro. En formacion no hace nada: el
// bebe al mando es de practica y no debe tocar la NVS (ADR-0002).
void WeighTracker_Tick(void);

// Una pesada real se da por registrada. 0 g (SKIP) y los pesos de bebes que no
// estan al mando se ignoran.
//
// Dos momentos, a proposito distintos:
//  - Asistente y pantalla Bebes NO esperan respuesta de la placa, asi que sellan
//    AL ENVIAR (`Communication_SendProfileWeight()`, tras sus guardas de
//    formacion). Si la trama se pierde, el aviso saldra de mas, no de menos.
//  - El dialogo de PESAR AHORA SI espera `CTRL,PROFILE_RANGE` (la unica
//    respuesta a un PROFILE_WEIGHT) y sella al recibirla; sin ella no sella,
//    para que un enlace caido no calle el aviso 24 h con un peso que no llego.
void WeighTracker_NoteWeighed(uint32_t seq, uint16_t grams);

// Hay que sacar el aviso ahora mismo (ver weigh_reminder_should_warn()).
bool WeighTracker_ShouldWarn(void);

// Horas desde la referencia, para el texto del aviso. 0 si no se sabe.
uint32_t WeighTracker_ElapsedHours(void);

// Boton MAS TARDE: calla el aviso 24 h. Sin hora no puede fecharlo.
void WeighTracker_Snooze(void);

// Interruptor de Ajustes (`wgh_en`, por defecto ON). Apagado no avisa, pero
// los sellos se siguen anotando. Quien lo llama rechaza antes en formacion.
bool WeighTracker_IsEnabled(void);
void WeighTracker_SetEnabled(bool on);

// Escribe en NVS lo pendiente. Llamar FUERA de LVGL_Lock.
void WeighTracker_Flush(void);

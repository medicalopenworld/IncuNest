#ifndef UI_ALARM_CENTER_H
#define UI_ALARM_CENTER_H

#include <lvgl.h>

// Centro de alarmas: alarmas activas + registro de las ultimas 10, con la
// misma presentacion de tarjetas que el historial de bebes (BabyHistory).
// Al pulsar una tarjeta se abre un pop-up con la descripcion de la alarma.
//
// Cuelga de lv_layer_top(), no de una pantalla concreta, para que sea
// accesible desde cualquier sitio — incluida la pantalla de bloqueo, que es
// donde el equipo pasa la mayor parte del tiempo.
void AlarmCenter_Init(void);

// Abre el centro de alarmas y pide el registro a la motherBoard.
// Reentrante: si ya esta abierto no hace nada.
void AlarmCenter_Open(void);

// Cierra el centro de alarmas. Idempotente.
void AlarmCenter_Close(void);

// true mientras esta visible. Lo consulta alarm_banner_update() para no pintar
// el banner encima del propio centro de alarmas.
bool AlarmCenter_IsOpen(void);

// Maquina de estados: llamar desde el bucle de UI.
void AlarmCenter_Poll(void);

#endif  // UI_ALARM_CENTER_H

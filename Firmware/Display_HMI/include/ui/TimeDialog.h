#pragma once
// Ajuste manual del reloj (HMI,SET_TIME, ver Firmware/PROTOCOL.md), abierto
// al tocar la hora de cabecera (ui_ClockButton, ui_ScreenMain).
//
// Pop-up modal con teclado numerico, mismo patron que el paso de peso del
// BabyWizard: la fecha y la hora arrancan EN BLANCO ("XX/XX/XX XX/XX") y el
// operador teclea los 10 digitos.
//
// Sustituye a los 5 spinboxes que se prefijaban con la hora que ya conocia el
// HMI. Prefijar invitaba a confirmar por reflejo una hora que nadie habia
// comprobado —justo el reflejo que este dialogo existe para romper—, y ese es
// el mismo motivo por el que el paso de peso del wizard tampoco prefija nada.
#include <lvgl.h>

// Crea overlay/tarjeta (ocultos). Llamar una vez durante la init de UI, con
// ui_ScreenMain como parent explicito (nunca lv_scr_act(): en ese momento la
// pantalla activa sigue siendo el splash).
void TimeDialog_Init(lv_obj_t *parent);

// Abre el dialogo con la mascara vacia. Lo llama ClockButton_cb.
void TimeDialog_Open(void);

// True mientras el dialogo este visible. Lo consultan los overlays que
// esperan turno (MaintenanceDialog) para no pintarse encima, y tambien el
// motor de lecciones (hmi-training-courses), junto con TimeDialog_Close().
bool TimeDialog_IsOpen(void);
void TimeDialog_Close(void);

// Consume el CTRL,TIME_ACK pendiente de CommTask y aplica la regla de "una
// alarma critica se lleva la pantalla por delante". Llamar una vez por vuelta
// de UI_Task, dentro de LVGL_Lock().
void TimeDialog_Poll(void);

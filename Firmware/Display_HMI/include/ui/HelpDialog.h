#pragma once
// Menu de ayuda (spec hmi-help-center), abierto desde el boton "?" del
// heading (ui_HelpButton, ui_ScreenMain).
//
// Pop-up modal con el patron de TimeDialog: overlay reutilizado, tarjeta con
// boton X y contenido reconstruible por vista. Tres vias:
//   - Tutorial guiado: cierra el dialogo y arranca HelpTour.
//   - Video tutorial: QR con SUPPORT_TUTORIAL_URL.
//   - Contacto: mensaje breve con teclado en pantalla; envio desde el equipo
//     (telemetria ThingsBoard, via tarea WiFi/OTA) y/o QR mailto: para el
//     movil del operador, con asunto e informe de depuracion ya rellenos.
#include <lvgl.h>

// Crea overlay/tarjeta (ocultos). Llamar una vez durante la init de UI, con
// ui_ScreenMain como parent explicito (nunca lv_scr_act(): en ese momento la
// pantalla activa sigue siendo el splash).
void HelpDialog_Init(lv_obj_t *parent);

// Abre el menu en su primera vista. Lo llama HelpButton_cb.
void HelpDialog_Open(void);
void HelpDialog_Close(void);

// Verdadero mientras el dialogo este visible. Lo consulta el temporizador de
// inactividad para no mandar la pantalla al bloqueo con la ayuda abierta.
bool HelpDialog_IsOpen(void);

// Consume el resultado del envio pendiente y aplica la regla de "una alarma
// critica se lleva la pantalla por delante". Llamar una vez por vuelta de
// UI_Task, dentro de LVGL_Lock().
void HelpDialog_Poll(void);

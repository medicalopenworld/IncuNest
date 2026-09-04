#pragma once
// Tutorial guiado (spec hmi-help-center): recorre los controles reales de la
// UI resaltando cada uno con un marco y un bocadillo explicativo, con
// ANTERIOR / SIGUIENTE / SALIR.
//
// Vive en lv_layer_top() para sobrevivir al cambio de pantalla (algunos pasos
// estan en ui_ScreenSettings) y para interceptar todos los toques: durante el
// recorrido no se acciona nada. Al salir o terminar devuelve ui_ScreenMain.
#include <lvgl.h>

// Crea el overlay (oculto). Llamar una vez durante la init de UI, despues de
// ui_init(): la tabla de pasos referencia los globales ui_* por puntero, asi
// que no hace falta que las pantallas esten cargadas, solo creadas.
void HelpTour_Init(void);

// Arranca desde el primer paso. Lo llama HelpDialog al elegir "Tutorial".
void HelpTour_Start(void);

// Cierra el recorrido y vuelve a ui_ScreenMain si hacia falta.
void HelpTour_Stop(void);

// Verdadero mientras el recorrido este en pantalla (exencion del auto-bloqueo).
bool HelpTour_IsOpen(void);

// Regla de "una alarma critica se lleva la pantalla por delante". Llamar una
// vez por vuelta de UI_Task, dentro de LVGL_Lock().
void HelpTour_Poll(void);

#ifndef UI_FACTORY_TEST_H
#define UI_FACTORY_TEST_H

#include <lvgl.h>

// Pantalla de test de fabrica (openspec/changes/shared-factory-test).
//
// Cuelga de lv_layer_top(), mismo molde que AlarmCenter.h: overlay oculto
// creado una vez, _Open/_Close/_IsOpen/_Poll para el ciclo de vida y
// _ApplyLanguage para el cambio de idioma en caliente. Solo se abre desde el
// boton "TEST FABRICA" del splash (ui_ScreenIntro); ninguna otra pantalla
// ofrece la entrada.
void FactoryTest_Init(void);

// Abre la pantalla y arranca la secuencia de tests locales. Reentrante: si ya
// esta abierto no hace nada.
void FactoryTest_Open(void);

// Cierra el overlay. Si hay una bateria de motherBoard en curso, envia
// HMI,FTEST,ABORT antes. Idempotente.
void FactoryTest_Close(void);

// true mientras esta visible. Lo consulta intro_timer_cb() (no navegar) e
// inactivity_timer_cb() (no bloquear) en UITask.cpp.
bool FactoryTest_IsOpen(void);

// Maquina de estados por polling: llamar desde el bucle de UI, dentro de
// LVGL_Lock() (igual que AlarmCenter_Poll/TelemetryHistory_Poll). Las
// operaciones de E/S que no deben retener el mutex de LVGL (I2C, NVS) se
// liberan y reafirman internamente con LVGL_Lock()/LVGL_Unlock().
void FactoryTest_Poll(void);

// Vuelve a fijar los textos visibles leyendo g_lang. Llamar desde
// UI_ApplyLanguage() (UITask.cpp), como TelemetryHistory_ApplyLanguage().
void FactoryTest_ApplyLanguage(void);

// Puesto por el callback del boton del splash; leido por intro_timer_cb()
// para no navegar a ui_ScreenMain mientras el operario esta en el test de
// fabrica. FactoryTest_Close() lo vuelve a poner a false.
extern bool g_factoryTestRequested;

#endif  // UI_FACTORY_TEST_H

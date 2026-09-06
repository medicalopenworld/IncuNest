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
//
// fromSettings (hmi-factory-test-settings-entry): true cuando la entrada es
// la fila "Test de hardware" de ui_ScreenSettings, en vez del boton del
// splash. Solo cambia el destino de FactoryTest_Close(): si el operario
// contesta "No" en la barrera de entrada (Step::Gate) sin haber arrancado
// ningun test, el overlay se cierra sin navegar a ui_ScreenMain — se queda en
// Settings, de donde vino. Cualquier cierre posterior a esa barrera (batalla
// completa o abortada) sigue cargando ui_ScreenMain como siempre.
void FactoryTest_Open(bool fromSettings = false);

// Hand-off puro (mismo patron que el resto de FactoryTest.cpp): la fila
// "Test de hardware" de ui_ScreenSettings solo marca aqui su intencion;
// FactoryTest_Poll() la resuelve llamando a FactoryTest_Open(true) en la
// siguiente pasada, incluso con el overlay cerrado.
void FactoryTest_RequestOpenFromSettings(void);

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

// Habilita/deshabilita y muestra/oculta el subtexto de aviso de la fila
// "Test de hardware" de ui_ScreenSettings segun UI_AnyControlActive().
// Llamar desde el bucle de UI_Task cuando lv_scr_act() == ui_ScreenSettings;
// no repinta si el estado no cambio desde la ultima pasada.
void FactoryTest_RefreshSettingsRow(void);

// Puesto por el callback del boton del splash; leido por intro_timer_cb()
// para no navegar a ui_ScreenMain mientras el operario esta en el test de
// fabrica. FactoryTest_Close() lo vuelve a poner a false.
extern bool g_factoryTestRequested;

#endif  // UI_FACTORY_TEST_H

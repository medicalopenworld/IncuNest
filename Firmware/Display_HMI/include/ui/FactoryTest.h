#ifndef UI_FACTORY_TEST_H
#define UI_FACTORY_TEST_H

#include <lvgl.h>

// Pantalla de test de fabrica (openspec/changes/shared-factory-test).
//
// Cuelga de lv_layer_top(), mismo molde que AlarmCenter.h: overlay oculto
// creado una vez, _Open/_Close/_IsOpen/_Poll para el ciclo de vida y
// _ApplyLanguage para el cambio de idioma en caliente. Entradas: la fila
// "Test de hardware" de ui_ScreenSettings (hmi-factory-test-settings-only) y,
// solo mientras el primer test de fabrica este pendiente, el boton de
// ui_ScreenIntro (ver FactoryTest_FirstTestPending()).
void FactoryTest_Init(void);

// Abre la pantalla y arranca la secuencia de tests locales. Reentrante: si ya
// esta abierto no hace nada.
//
// La entrada es siempre la fila "Test de hardware" de ui_ScreenSettings
// (hmi-factory-test-settings-only): si el operario contesta "No" en la
// barrera de entrada (Step::Gate) sin haber arrancado ningun test, el overlay
// se cierra sin navegar a ui_ScreenMain — se queda en Settings, de donde
// vino. Cualquier cierre posterior a esa barrera (bateria completa o
// abortada) sigue cargando ui_ScreenMain como siempre.
void FactoryTest_Open(void);

// Hand-off puro (mismo patron que el resto de FactoryTest.cpp): la fila
// "Test de hardware" de ui_ScreenSettings solo marca aqui su intencion;
// FactoryTest_Poll() la resuelve llamando a FactoryTest_Open() en la
// siguiente pasada, incluso con el overlay cerrado.
void FactoryTest_RequestOpenFromSettings(void);

// Interruptor del primer test de hardware de fabrica (2026-10-06: aparcado
// para mas adelante). A 0, FactoryTest_FirstTestPending() es siempre false:
// el equipo arranca como siempre, no se lee ni se escribe la marca de NVS y
// el test de Ajustes funciona como antes. Ponerlo a 1 reactiva todo lo de
// abajo. OJO al reactivarlo: las placas que hayan arrancado con este firmware
// ya tienen el contador de arranques y se tomaran por equipos antiguos.
#ifndef FACTORY_FIRST_HW_TEST_ENABLED
#define FACTORY_FIRST_HW_TEST_ENABLED 0
#endif

// Primer test de hardware de fabrica. Mientras este pendiente, el equipo se
// queda en ui_ScreenIntro con el boton "TEST DE HARDWARE" y no pasa a
// ui_ScreenMain. En ese primer test SIM ACT tiene que PASAR (un AVISO u
// OMITIDO cuenta como error), GSM SIGNAL no se muestra ni cuenta, y el
// veredicto bueno es "TEST OK". Solo un TEST OK lo marca hecho en NVS.
//
// LoadFirstTestState() se llama UNA vez desde setup(), antes de crear las
// tareas. `legacyUnit`: la NVS ya traia el contador de arranques de un
// firmware anterior; es un equipo que ya estaba en uso y recibe este
// firmware por OTA, y no se le exige el test (quedaria parado en la pantalla
// de inicio en pleno servicio).
void FactoryTest_LoadFirstTestState(bool legacyUnit);
bool FactoryTest_FirstTestPending(void);
// Boton de ui_ScreenIntro: mismo hand-off que la fila de Settings.
void FactoryTest_RequestOpenFirstTest(void);

// Cierra el overlay. Si hay una bateria de motherBoard en curso, envia
// HMI,FTEST,ABORT antes. Idempotente.
void FactoryTest_Close(void);

// true mientras esta visible. Lo consulta inactivity_timer_cb() (no
// bloquear) en UITask.cpp.
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

#endif  // UI_FACTORY_TEST_H

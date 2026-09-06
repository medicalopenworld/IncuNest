#ifndef UI_TELEMETRY_HISTORY_H
#define UI_TELEMETRY_HISTORY_H

// Tendencia de temperatura de aire/piel y humedad, con ventana de tiempo
// seleccionable (1 h por defecto / 2 h / 4 h). Mismo criterio que
// AlarmCenter.h: cuelga de lv_layer_top(), no de una pantalla concreta, para
// ser accesible desde ui_ScreenLock sin desbloquear el equipo.
void TelemetryHistory_Init(void);

// Abre la vista y repinta al instante: los datos ya estan en el buffer local
// (TelemetryHistory_RecordSample los va acumulando en segundo plano), no hay
// nada que pedirle a la motherBoard. Reentrante: si ya esta abierto no hace
// nada.
void TelemetryHistory_Open(void);

// Cierra si hay una alarma activa o el enlace HMI<->motherBoard esta caido:
// a diferencia de BabyHistory_Poll, no basta con una alarma CRITICA — este
// panel no tiene informacion de alarma propia que compense tapar el banner o
// el icono de AUDIO PAUSED (ambos en lv_layer_top(), igual que este overlay).
// Llamar desde el bucle de UI.
void TelemetryHistory_Poll(void);

// Anade una muestra al buffer circular (submuestreado internamente por
// tiempo real, ~1 cada 10 s, 4 h de techo). Llamar cada vez que llega
// telemetria nueva, este la vista abierta o no: el buffer sigue lleno para
// cuando se abra. airOk/skinOk/humOk: false si esa medida es el centinela
// PROTO_TEL_*_UNAVAILABLE de PROTOCOL.md o si el enlace esta caido — nunca
// se guarda ni se pinta un centinela como si fuera una lectura real.
void TelemetryHistory_RecordSample(float airTempC, bool airOk,
                                    float skinTempC, bool skinOk,
                                    float humPct, bool humOk);

// Vuelve a fijar el titulo y las etiquetas de canal ("AIRE"/"PIEL"/
// "HUMEDAD") leyendo g_lang. Necesario porque esos labels solo se pintan una
// vez, en TelemetryHistory_Init() — sin este enganche se quedarian en el
// idioma de arranque tras un cambio en caliente desde Settings. Llamar desde
// UI_ApplyLanguage() (UITask.cpp), como ya hace photo_safety_apply_language.
void TelemetryHistory_ApplyLanguage(void);

// Para el motor de lecciones (hmi-training-courses).
bool TelemetryHistory_IsOpen(void);
void TelemetryHistory_Close(void);

#endif  // UI_TELEMETRY_HISTORY_H

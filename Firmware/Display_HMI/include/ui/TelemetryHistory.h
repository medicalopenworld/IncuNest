#ifndef UI_TELEMETRY_HISTORY_H
#define UI_TELEMETRY_HISTORY_H

#include <stdint.h>

// Tendencia de temperatura de aire/piel y humedad, con ventana de tiempo
// seleccionable (4 h por defecto / 12 h / 24 h). El eje X es SIEMPRE la ventana
// completa, con "ahora" en el borde derecho: rejilla vertical cada 15 min /
// 1 h / 2 h en los tres charts y, bajo el de humedad, 5 horas (HH:MM local si
// la placa ha mandado la hora; si no, "-h:mm" hacia atras). Mismo criterio que
// AlarmCenter.h: cuelga de lv_layer_top(), no de una pantalla concreta.
//
// El buffer es de 24 h y SOBREVIVE a un reinicio o un corte de red: se vuelca a
// LittleFS (/trend.bin) y se restaura alineado por epoch en cuanto hay hora.
//
// Sobre cada grafico: la consigna historica (linea morada) y una marca
// vertical por cada flanco de alarma (alta: continua, color de la prioridad;
// baja: discontinua verde), para correlacionar sin salir de la pantalla.
//
// Punto de entrada: el boton TENDENCIA de la cabecera de AlarmCenter (ver
// openspec/changes/hmi-trend/design.md).
void TelemetryHistory_Init(void);

// Abre la vista y repinta al instante: los datos ya estan en el buffer local
// (TelemetryHistory_RecordSample los va acumulando en segundo plano), no hay
// nada que pedirle a la motherBoard. Reentrante: si ya esta abierto no hace
// nada. No abre si hay una alarma activa o el enlace con la placa esta caido.
void TelemetryHistory_Open(void);

// Cierra si hay una alarma activa o el enlace HMI<->motherBoard esta caido:
// a diferencia de BabyHistory_Poll, no basta con una alarma CRITICA — este
// panel no tiene informacion de alarma propia que compense tapar el banner o
// el icono de AUDIO PAUSED (ambos en lv_layer_top(), igual que este overlay).
// Con la vista abierta repinta ademas una vez cada 10 s para que el eje
// avance con el reloj aunque no llegue telemetria. Drena tambien los flancos
// de alarma encolados por CommTask, este abierta la vista o no. Llamar desde
// el bucle de UI, bajo LVGL_Lock.
void TelemetryHistory_Poll(void);

// Anade una muestra al buffer circular: una ranura de 10 s de reloj
// (esp_timer) por muestra; las ranuras sin telemetria se rellenan como "sin
// dato" para que el indice del buffer siga siendo el tiempo. Llamar cada vez
// que llega telemetria nueva, este la vista abierta o no: el buffer sigue
// lleno para cuando se abra. xxxOk: false si esa medida es el centinela
// PROTO_TEL_*_UNAVAILABLE de PROTOCOL.md o si el enlace esta caido — nunca se
// guarda ni se pinta un centinela como si fuera una lectura real.
//
// spXxx/spXxxOk: la consigna de cada canal. spXxxOk = false cuando esa
// magnitud no esta bajo control (ver trend_setpoint_validity): no se pinta una
// linea de objetivo que el equipo no persigue.
void TelemetryHistory_RecordSample(float airTempC, bool airOk,
                                    float skinTempC, bool skinOk,
                                    float humPct, bool humOk,
                                    float spAirC, bool spAirOk,
                                    float spSkinC, bool spSkinOk,
                                    float spHumPct, bool spHumOk);

// Flanco de alarma (la condicion pasa de inactiva a activa, o al reves).
// Contexto: CommTask, sin LVGL_Lock. Solo encola (hand-off): la UI lo convierte
// a ranura con el instante del flanco. id = ALARMS_ID, priority = 0 baja,
// 1 media, 2 alta (PROTOCOL.md, CTRL,ALM), raised = true en la alta.
void TelemetryHistory_RecordAlarmEdge(uint8_t id, uint8_t priority,
                                      bool raised);

// ---- Persistencia (/trend.bin en LittleFS) ---------------------------------

// Pide un volcado en cuanto la politica lo permita: se llama al ENTRAR EN
// BLOQUEO (enter_lock_screen y la ruta de auto-bloqueo). Solo levanta una
// bandera: seguro bajo LVGL_Lock. La politica (>= 2 min desde el ultimo
// volcado) decide si se atiende ya o mas tarde.
void TelemetryHistory_NoteLockEntered(void);

// Una pasada del servicio de persistencia. Llamar en CADA vuelta del bucle de
// UI_Task y FUERA de LVGL_Lock (ARQ-LOCK-001: es E/S de flash). Con la hora
// sincronizada: restaura el fichero una vez (anteponiendolo a lo ya registrado)
// y vuelca el modelo cada 5 min / al bloquear, en trozos de 4 KB espaciados
// >= 50 ms sobre un temporal que se renombra al final (atomico). Todo va por
// tramos acotados (reserva, serializacion, CRC y restauracion incluidas): cada
// pasada hace como mucho UN tramo. Y NO hay E/S mientras haya una alarma activa
// (de cualquier prioridad) o el enlace con la placa este caido: un volcado a
// medias se aborta limpio (el temporal se descarta), una restauracion se pausa,
// y el formateo diferido de la particion (hmi_fs) espera igual.
void TelemetryHistory_PersistService(void);

// Vuelve a fijar el titulo, las etiquetas de canal ("AIRE"/"PIEL"/
// "HUMEDAD") y la leyenda leyendo g_lang. Necesario porque esos labels solo
// se pintan una vez, en TelemetryHistory_Init() — sin este enganche se
// quedarian en el idioma de arranque tras un cambio en caliente desde
// Settings. Llamar desde UI_ApplyLanguage() (UITask.cpp), como ya hace
// photo_safety_apply_language.
void TelemetryHistory_ApplyLanguage(void);

// Para el motor de lecciones (hmi-training-courses).
bool TelemetryHistory_IsOpen(void);
void TelemetryHistory_Close(void);

#endif  // UI_TELEMETRY_HISTORY_H

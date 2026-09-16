#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "alarm_ids.h"
#include "control_types.h"

typedef enum {
  SKIN_PROBE_NOT_CONNECTED = 0,
  SKIN_PROBE_PENDING_VALIDATION,
  SKIN_PROBE_VALID,
  SKIN_PROBE_INVALID,
  SKIN_PROBE_OUT_OF_RANGE,
  SKIN_PROBE_DISCONNECTED_DURING_OPERATION,
  SKIN_PROBE_UNSTABLE,
} SkinProbeState;

// Centinelas de "medida no disponible" en CTRL,TEL.
//
// Un sensor caido enviaba 0, y 0 es un valor PLAUSIBLE: quien mira la pantalla
// lee "0.0 C" como una medida real y alarmante en vez de como la ausencia de
// medida que es. El enlace caido ya se pintaba como "--" por ese mismo motivo
// (ver link_lost_blank_update en el display); esto extiende el criterio al
// fallo de sensor, para que "no se sabe" se vea igual venga de donde venga.
//
// Fuera de cualquier rango fisico posible, para que no puedan confundirse con
// una lectura ni sobrevivir a un parseo descuidado.
#define PROTO_TEL_TEMP_UNAVAILABLE (-999.0)
#define PROTO_TEL_HUM_UNAVAILABLE  (-1)

// Comparacion de igualdad sobre un double que ha ido y vuelto por "%.1f": la
// tolerancia evita depender de la representacion exacta tras el formateo.
#define PROTO_TEL_TEMP_IS_UNAVAILABLE(v) ((v) < -900.0)

typedef struct {
  double detectedAirTemperature;
  double detectedSkinTemperature;
  double detectedHumidity;
  int    serverCommStatus;
  int    serialNumber;
} Proto_CtrlTelemetry;

typedef struct {
  int      actuation;
  int      controlMode;
  double   desiredAirTemperature;
  double   desiredSkinTemperature;
  double   desiredHumidity;
  int      phototherapyMode;
  int      muteAlarm;
  int      serialNumber;
  int      hwNum;
  char     hwRev[2];
  char     fwVer[20];
  int      language;
  int      skinModeEnabled;
  int      serverCommStatus;
  int      photoMinutesRemaining;
  int      photoSecondsRemaining;
  uint32_t alarmBitmask;
  int      skinProbeState;
  // Barras de cobertura (0-4) del transporte activo indicado por
  // serverCommStatus, derivadas de RSSI (WiFi) o CSQ (GPRS). -1 = sin dato
  // fiable: serverCommStatus == COMM_STATUS_NONE (no hay transporte del que
  // medir cobertura) o una placa antigua que no manda este campo todavia.
  int      linkBars;
} Proto_CtrlState;

typedef struct {
  int  id;
  char type[ALARM_TITLE_MAX_CHARS + 1];
  char description[ALARM_DESC_MAX_CHARS + 1];
  uint8_t state;
  // Prioridad resuelta por la motherBoard (AlarmPriority). Viaja por el cable
  // en vez de deducirse en el display: la placa es la dueña de la informacion
  // de alarmas y el display se limita a pintarla, asi que no debe haber una
  // segunda copia de la politica de prioridades esperando a desincronizarse.
  uint8_t priority;
} Proto_CtrlAlarm;

typedef struct {
  uint8_t ppg;
} Proto_CtrlPPG;

typedef struct {
  uint8_t hr;
  uint8_t spo2;
} Proto_CtrlVitals;

typedef struct {
  SkinProbeState state;
} Proto_CtrlProbe;

typedef struct {
  int    actuation;
  int    controlMode;
  double desiredAirTemperature;
  double desiredSkinTemperature;
  double desiredHumidity;
  int    phototherapyMode;
  int    muteAlarm;
  int    language;
  int    skinModeEnabled;
  int    photoMinutesRemaining;
} Proto_HmiCommand;

// Rango de la fuente que fijo el reloj de pared. Viaja en el campo `src` de
// CTRL,TIME, asi que es vocabulario del protocolo y no un detalle interno de
// la motherBoard: el HMI lo necesita para decidir si lo que recibe merece
// escribirse en su RTC.
//
// GANA EL MAYOR. Se numera en orden creciente de confianza a proposito: en
// codigo `nueva > vigente` se lee solo, mientras que numerar al reves —el 1
// como el mejor, que es como se suele hablar de prioridades— invita al error
// de signo cada vez que alguien toca la comparacion.
//
// NO es la misma escala que TzSource, aunque compartan forma. TzSource ordena
// el HUSO y tiene IP en el 2, sin NTP ni RTC; esta ordena el INSTANTE. El
// protocolo las transmite en campos distintos (`tzsrc` y `src`) y fundirlas
// costaria un bug el dia que alguien pase un valor de una a la otra.
typedef enum {
  // No se sabe. Con el reloj sin sincronizar, o con un CTRL,TIME de una
  // motherBoard anterior a esta version, que no envia el campo.
  PROTO_TIME_SOURCE_NONE = 0,
  // NITZ de la red movil. Va el ultimo porque muchos operadores no lo emiten,
  // o lo emiten con minutos de error. Sigue siendo la MEJOR fuente de huso
  // (ver TzSource): es la peor hora y la mejor zona, y por eso son dos
  // escalas separadas.
  PROTO_TIME_SOURCE_NITZ = 1,
  // El PCF8563 del HMI. Conserva la hora entre apagados, que es justo lo que
  // ninguna otra fuente hace, pero es un RTC de cristal sin compensacion
  // termica: deriva minutos al mes. Sirve de semilla, no de referencia.
  PROTO_TIME_SOURCE_RTC = 2,
  // NTP/SNTP, por WiFi o por el contexto PDP del modem. Precision de segundos
  // y fecha fiable.
  PROTO_TIME_SOURCE_NTP = 3,
  // La tecleo el operador, en /config o en HMI,SET_TIME. Gana a todo: es la
  // unica que conoce la hora local sin red, y desplazarla bajo los pies de
  // quien la acaba de poner es peor que un error de minutos.
  PROTO_TIME_SOURCE_MANUAL = 4,
} Proto_TimeSource;

#ifdef __cplusplus
}
#endif

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

#ifdef __cplusplus
}
#endif

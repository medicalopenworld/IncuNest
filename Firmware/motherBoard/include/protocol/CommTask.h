#pragma once
#include <Arduino.h>
#include "protocol.h"
#include "control_types.h"
#include "alarm_ids.h"
#include "factory_test.h"

#if IS_HMI
#define EXPECTED_PREFIX "CTRL"
#else
#define EXPECTED_PREFIX "HMI"
#endif

// Skin probe values — now in SkinProbeState enum in protocol.h.
// These aliases maintain backward compatibility with existing MB code.
#define SKIN_PROBE_NOT_CONNECTED  0
#define SKIN_PROBE_VALID          2

#define DUTY_MSG_BUF_SIZE 24

// Backward-compatibility aliases so callers compile unchanged.
// TelemetryMessage and HMI_CommandMessage are the MB-local wrappers
// that add internal flags on top of the shared protocol types.
typedef struct {
  double detectedAirTemperature;
  double detectedSkinTemperature;
  double detectedHumidity;
  int    serverCommStatus;
  int    serialNumber;
  // Barras de cobertura (0-4) del transporte activo en serverCommStatus, o
  // -1 si no hay transporte o el dato de senal (RSSI/CSQ) no es fiable
  // todavia. Se computa junto a serverCommStatus y viaja al HMI por
  // CTRL,STATE (no por CTRL,TEL: ver send_state_to_hmi()).
  int    linkBars;
} TelemetryMessage;

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
  // MB-internal flags (not part of the protocol)
  bool   newCommand;
} HMI_CommandMessage;

extern TelemetryMessage   ctrl_tel_msg;
extern HMI_CommandMessage hmi_cmd_msg;

void   CommunicationHost_Init();
void   Communication_Task(void *pv);
void   CommunicationHost_Send(const char *);
void   setHMIConnected(bool connected);
double getRemainingPhotoTime();
void   sendWifiToHMI(const char *ssid, const char *pass);

// Cola de lineas hacia el HMI para tareas que no son Communication_Task
// (design.md D3, shared-factory-test): solo Communication_Task escribe en
// hmiSerial, para no entrelazar bytes con CTRL,PPG/CTRL,TEL a 25/1 Hz. Copia
// `line` (debe caber en FTEST_TX_LINE_MAX, incluido el terminador nulo) y
// devuelve enseguida; con la cola llena, la linea se descarta con log de
// error y NUNCA bloquea a quien llama.
void CommunicationHost_Enqueue(const char *line);

// true si se ha visto alguna vez una linea del HMI y la ultima llego hace
// como mucho `max_silence_ms` (encapsula g_lastHmiLineMs/g_hmiEverSeen,
// parse_line() en CommTask.cpp, para quien necesite un dead-man sin tocar
// esos globales directamente). Devuelve true tambien si nunca se ha visto
// ninguna linea: "nunca conectado" no es lo mismo que "se dejo de hablar",
// mismo criterio que checkHmiLink() (security.cpp). Usada por el dead-man
// del test de fabrica (design.md D4, shared-factory-test).
bool CommunicationHost_HmiAlive(uint32_t max_silence_ms);

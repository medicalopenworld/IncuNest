#ifndef COMM_TASK_H
#define COMM_TASK_H

#include "main.h"
#include <Arduino.h>
#include <lvgl.h>
#include "protocol.h"
#include "control_types.h"
#include "alarm_ids.h"
#include "factory_test.h"

#define COMMUNICATION_DEBUG true
#if COMMUNICATION_DEBUG
#define COMM_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define COMM_LOG(...)
#endif

#define COMM_SERIAL Serial

// Cadencia del latido del display hacia la placa. La placa declara
// ALARM_HMI_LINK_LOST tras HMI_LINK_TIMEOUT_MS sin recibir nada; el margen
// entre ambos (5 tramas) evita declararlo por un hueco puntual de la UART.
#define HMI_KEEPALIVE_PERIOD_MS 1000u

// Y al reves: silencio de la PLACA visto desde el display. La motherBoard
// emite CTRL,STATE y CTRL,TEL cada 1 s, asi que 5 s son cinco tramas — la
// misma ventana que usa ella para declarar ALARM_HMI_LINK_LOST. Simetrica a
// proposito: un solo numero que recordar y los dos extremos cuentan igual.
#define BOARD_LINK_TIMEOUT_MS 5000u

// Margen desde el arranque del display antes de dar por ausente una placa que
// no ha hablado NUNCA. Sin el, "todavia no ha llegado la primera linea" era un
// estado benigno para siempre y arrancar sin el cable no producia ningun aviso.
//
// 5000 (minimo del splash de arranque) + BOARD_LINK_TIMEOUT_MS: no introduce un
// numero nuevo, se deriva de los dos que ya rigen el arranque y el silencio.
// Nota: el splash se estira hasta 15 s cuando no llega CTRL,STATE (intro_timer_cb),
// asi que sin cable el aviso ya esta puesto cuando la pantalla principal aparece.
//
// +5000 extra: en banco se ha visto que un arranque normal (con placa
// presente) a veces tarda algo mas de lo habitual en emitir su primera linea,
// y el margen justo disparaba un "LINK LOST" fantasma que se autocorregia
// segundos despues. Solo afecta a este primer margen de arranque, no al
// timeout de silencio en marcha (BOARD_LINK_TIMEOUT_MS).
#define BOARD_LINK_BOOT_GRACE_MS (5000u + 5000u + BOARD_LINK_TIMEOUT_MS)

// true si la placa lleva BOARD_LINK_TIMEOUT_MS sin decir nada, o si no ha dicho
// nada en absoluto pasado BOARD_LINK_BOOT_GRACE_MS desde el arranque. Mientras
// lo sea, las cifras en pantalla estan MUERTAS y no deben mostrarse como si
// fueran medidas actuales.
bool Display_IsBoardLinkLost(void);

// true en cuanto ha llegado una sola linea valida de la placa. Distinto de
// "el enlace esta vivo": sirve para no afirmar nada sobre un equipo del que
// todavia no se sabe nada, como al arrancar el display sin motherBoard.
bool Display_BoardEverSeen(void);

// Expected prefix of incoming messages
#if IS_HMI
#define EXPECTED_PREFIX "CTRL"
#else
#define EXPECTED_PREFIX "HMI"
#endif

// Backward-compatibility aliases for existing HMI code.
// ControlBoard_Message_Telemetry and ControlBoard_Message_Alarm map 1:1
// to the shared protocol types.
typedef Proto_CtrlTelemetry   ControlBoard_Message_Telemetry;
typedef Proto_CtrlAlarm       ControlBoard_Message_Alarm;

// HMI_Message: protocol fields from Proto_HmiCommand plus HMI-internal flag.
typedef struct {
  // Protocol fields (matching Proto_HmiCommand)
  int    actuation;
  int    controlMode;
  double desiredAirTemperature;
  double desiredSkinTemperature;
  double desiredHumidity;
  int    phototherapyMode;
  bool   muteAlarm;
  int    language;
  bool   skinModeEnabled;
  int    photoMinutesRemaining;
  // HMI-internal flag (not part of the protocol)
  bool   shouldSendData;
} HMI_Message;

// ControlBoard_Message_State: protocol fields from Proto_CtrlState plus
// HMI-internal flag.
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
  // Bit por AlarmId de las condiciones en AUDIO PAUSED. La placa es la dueña
  // de este estado: la pausa caduca sola (60601-2-19 201.12.3.104) y el
  // display no puede saberlo por su cuenta.
  uint32_t silencedBitmask;
  // Prioridad que reproduce la prueba de funcionamiento de alarmas
  // (201.12.3.105), o ALARM_TEST_IDLE_HMI si no hay prueba en curso.
  int      alarmTestPriority;
  // Segundos hasta que vuelva el audio de la pausa que expira antes, 0 si no
  // hay ninguna condicion silenciada. Alimenta la cuenta atras que se pinta
  // junto al icono de AUDIO PAUSED.
  int      silenceRemainingS;
  int      skinProbeState;
  // Barras de cobertura (0-4) del transporte activo en serverCommStatus, o
  // -1 si no hay transporte, el dato de senal no es fiable, o la placa es
  // antigua y no manda este campo. Alimenta el indicador de cobertura del
  // heading (ver connectivity_heading_update() en UITask.cpp).
  int      linkBars;
  // HMI-internal flag (not part of the protocol)
  bool     newState;
} ControlBoard_Message_State;

// Legacy name for the probe state enum
typedef SkinProbeState ProbeContactState;
// SPO2_PROBE_* aliases (HMI code uses these names)
// El contrato numerico lo fija ProbeState de la libreria incunest_afe4490 (la
// motherboard reenvia el valor crudo en CTRL,PROBE): 0=DISCONNECTED,
// 1=NOT_APPLIED, 2=APPLIED, 3=SATURATING. Si la libreria anade un estado
// nuevo, hay que anadirlo aqui y en PROTOCOL.md: el HMI trata cualquier
// estado desconocido como "sin contacto" (fail-safe), no lo descarta.
#define SPO2_PROBE_DISCONNECTED SKIN_PROBE_NOT_CONNECTED
#define SPO2_PROBE_NOT_APPLIED  SKIN_PROBE_PENDING_VALIDATION
#define SPO2_PROBE_APPLIED      SKIN_PROBE_VALID
// Canal saturado desde arriba (exceso de luz): la presencia de tejido es
// DESCONOCIDA, por lo que no implica APPLIED. Es el estado tipico al retirar
// la sonda, que queda expuesta a la luz ambiente.
#define SPO2_PROBE_SATURATING   SKIN_PROBE_INVALID

// PPG waveform sample (CTRL,PPG — 25 Hz)
typedef struct {
  uint8_t ppg;     // normalised 0-255
  bool    updated; // true after each new sample, cleared by consumer
} ControlBoard_Message_PPG;

// Vital signs (CTRL,VIT — 1 Hz)
typedef struct {
  uint8_t hr;   // 40-240 bpm; 0 = no valid signal
  uint8_t spo2; // 0-100 %; 0 = no valid signal
  float   pi;   // Perfusion Index [%]; 0.0 = no valid signal
  bool    updated;
} ControlBoard_Message_VIT;

// SpO2 probe contact state
typedef struct {
  ProbeContactState state;
  bool              updated;
} ControlBoard_Message_Probe;

// Sensor data message (HMI-internal, not a protocol type)
typedef struct {
  double temperature[3];
  double humidity[2];
  bool   shouldSendData;
} ControlBoard_Message;

// ======================
//   POWER OFF STATE
// ======================
extern volatile bool g_pwrOffActive;
extern volatile int  g_pwrOffRemainingMs;
constexpr int PWR_OFF_TOTAL_MS = 3000;

// ======================
//   GLOBAL VARIABLES
// ======================
extern HMI_Message                    hmi_msg;
extern ControlBoard_Message           ctrl_msg;
extern ControlBoard_Message_Telemetry ctrl_tel_msg;
extern ControlBoard_Message_Alarm     ctrl_msg_alarm;
extern ControlBoard_Message_State     ctrl_state_msg;
extern ControlBoard_Message_PPG       ctrl_ppg_msg;
extern ControlBoard_Message_VIT       ctrl_vit_msg;
extern ControlBoard_Message_Probe     ctrl_probe_msg;
extern int  g_skinProbeState;
extern bool error;
extern volatile bool g_pendingTelemetryApply;
extern portMUX_TYPE  g_telemetry_mux;
extern volatile int  g_tempDutyPwm;
extern volatile int  g_humDutyPwm;
extern volatile bool g_pendingDutyApply;

// ======================
//   BABY PROFILE WIZARD PROTOCOL (temp-control-activation-wizard)
// ======================
struct BabyProfileListItem {
  uint32_t seq;
  char     name[24];
  uint8_t  gestWeeks;
  uint16_t weightGrams;
  uint16_t kangarooCount;      // times taken out to the mother
  uint32_t phototherapyMinutes;// accumulated exposure for this baby
  uint32_t thermoMinutes;      // accumulated thermal-control time
  uint32_t humidityMinutes;    // accumulated humidity-control time
};
struct BabyProfileListMsg {
  int                  count; // 0-3
  BabyProfileListItem  items[3];
};
struct BabyProfileRangeMsg {
  uint32_t seq;
  bool     ageKnown;
  uint16_t ageDays;
  float    lo, hi, mid;
  bool     estimated;
};

extern volatile bool     g_pendingProfileList;
extern BabyProfileListMsg g_profileList;
extern volatile bool     g_pendingProfileAck;
extern uint32_t          g_profileAck;

// --- Wall clock, owned by the motherBoard (CTRL,TIME) ---------------------
// The HMI has no RTC and does no NTP of its own, so the motherBoard's synced
// epoch is the only clock available here. Returns 0 while the motherBoard
// reports "not synced" (or before the first CTRL,TIME arrives).
// Interpolates with millis() between the 10 s broadcasts.
uint32_t HMI_GetEpochNow();

// --- Zona horaria, tambien propiedad de la motherBoard --------------------
// La hora UTC es medible; la hora local NO lo es: es una convencion politica,
// asi que la placa la aprende de la red movil (NITZ) o de una consulta por IP
// y la difunde en CTRL,TIME. Todo lo que se ALMACENA o se TRANSMITE sigue en
// UTC — esto solo se aplica al formatear para una persona.
int8_t   HMI_GetTzQuarterHours();
// false mientras falte la hora o la zona. Offset 0 sin fuente NO es UTC+0.
bool     HMI_HasLocalTime();
// Epoch UTC -> segundos en hora local, solo para formatear.
uint32_t HMI_ToLocal(uint32_t utcEpoch);

// Ajuste manual del reloj desde la pantalla de Settings (ver HMI,SET_TIME en
// PROTOCOL.md). Llega por UART, no por WiFi, asi que funciona sin conexion.
void Communication_SendSetTime(int year, int month, int day, int hour,
                               int minute);
extern volatile bool g_pendingTimeAck;
extern uint32_t      g_timeAckResult; // 0 = aceptada, 1 = rechazada

extern volatile bool     g_pendingProfileRange;
extern BabyProfileRangeMsg g_profileRange;

// ======================
//   BABY HISTORY VIEWER PROTOCOL (baby-history-viewer)
// ======================
struct BabyHistoryItem {
  uint32_t seq;
  char     name[24];
  uint8_t  gestWeeks;
  uint16_t lastWeightGrams;
  uint32_t admissionEpoch;  // 0 = unknown
  uint32_t dischargeEpoch;  // 0 = never explicitly discharged
  uint8_t  outcome;         // 0=Unknown 1=Survived 2=Deceased 3=Transferred
  // Only meaningful when outcome==2 (Deceased); 0 otherwise. See PROTOCOL.md
  // BabyCause for the 1-6 mapping.
  uint8_t  cause;
  uint16_t kangarooCount;
  uint32_t phototherapyMinutes;
  uint32_t thermoMinutes;
  uint32_t humidityMinutes;
};
struct BabyHistoryMsg {
  uint32_t page;
  uint32_t totalCount;
  int      count;  // 0-10 entries in this page
  BabyHistoryItem items[10];
};
struct BabyWeightHistoryMsg {
  uint32_t seq;
  int      count;  // 0-50 points
  uint16_t dayOffset[50];
  uint16_t weightGrams[50];
};

// Registro de alarmas (IEC 60601-1-8 6.12.2). Llega entero de la motherBoard,
// titulo incluido: aqui no se traduce ni se deduce nada, solo se pinta.
struct AlarmHistoryItem {
  uint8_t  id;
  uint8_t  priority;
  // Campo propio, NO deducible de clearedEpoch != 0: sin hora sincronizada la
  // placa guarda clearedEpoch = 0 tambien al resolverse, y una alarma resuelta
  // quedaba indistinguible de una viva.
  bool     resolved;
  uint32_t raisedEpoch;   // 0 = la placa no tenia hora sincronizada
  uint32_t clearedEpoch;  // 0 = la placa no tenia hora sincronizada
  int16_t  limitCenti;    // limite en vigor x100, 0 si no aplica
  int16_t  valueCenti;    // medida que la disparo x100, 0 si no aplica
  char     title[ALARM_TITLE_MAX_CHARS + 1];
};
struct AlarmHistoryMsg {
  int count;  // 0..10
  AlarmHistoryItem items[10];
};

// Detalle de una alarma concreta (CTRL,ALM_DESC). No viaja dentro del
// historial porque 10 descripciones no caben en una linea; se pide bajo
// demanda al abrir el detalle. La placa lo manda ya traducido.
struct AlarmDescMsg {
  uint8_t id;
  char    title[ALARM_TITLE_MAX_CHARS + 1];
  char    desc[ALARM_DESC_MAX_CHARS + 1];
};

extern volatile bool        g_pendingAlarmHistory;
extern AlarmHistoryMsg      g_alarmHistory;
void Communication_SendAlarmHistoryReq(void);

extern volatile bool        g_pendingAlarmDesc;
extern AlarmDescMsg         g_alarmDesc;
void Communication_SendAlarmDescReq(uint8_t id);

// AUDIO PAUSED de UNA condicion. on=false lo cancela, que es lo que exige
// 60601-1-8 6.8.4 ("means to terminate any ALARM SIGNAL inactivation state").
void Communication_SendAlarmSilence(uint8_t id, bool on);

// Valor de alarmTestPriority cuando no hay prueba en curso. Espeja
// ALARM_TEST_IDLE de la motherBoard, que el display no puede incluir por vivir
// en motherBoard/src.
#define ALARM_TEST_IDLE_HMI 0xFF

// Lanza la prueba de funcionamiento de las senales de alarma (60601-2-19
// 201.12.3.105). La placa la rechaza si hay alguna alarma en curso.
void Communication_SendAlarmTest(void);

extern volatile bool        g_pendingBabyHistory;
extern BabyHistoryMsg       g_babyHistory;
extern volatile bool        g_pendingWeightHistory;
extern BabyWeightHistoryMsg g_weightHistory;

// --- Test de fabrica (CTRL,FTEST* / HMI,FTEST,*, shared-factory-test) ---
//
// La motherBoard manda una linea por CADA cambio de estado de un test
// (design.md D2/D9): CommTask escribe en un anillo con el mutex tomado y
// FactoryTest_Poll() lo drena entero cada pasada (patron g_pendingAlarmHistory,
// pero con cola en vez de un unico buffer porque aqui SI puede llegar mas de
// una linea entre dos pasadas de UI_Task).
#define FTEST_RING_LEN 8
struct FtestRing {
  FtestResult buf[FTEST_RING_LEN];
  uint8_t     head;   // siguiente a leer
  uint8_t     count;  // pendientes de leer
};
extern portMUX_TYPE  g_ftestMux;
extern volatile bool g_pendingFtest;
extern FtestRing     g_ftestRing;

// Saca el resultado mas antiguo del anillo a `*out`. false si no hay ninguno
// pendiente (no toca `*out`). Llamar solo desde FactoryTest_Poll().
bool FactoryTest_TakeEvent(FtestResult *out);

// CTRL,FTEST_DONE — cierre de bateria o de un test unico (design.md D2).
extern volatile bool     g_pendingFtestDone;
extern volatile unsigned g_ftestDonePass;
extern volatile unsigned g_ftestDoneFail;
extern volatile unsigned g_ftestDoneSkip;
extern volatile unsigned g_ftestDoneWarn;

// CTRL,FTEST_REJECT — no se pudo arrancar la bateria (o el test unico).
extern volatile bool g_pendingFtestReject;
extern volatile int  g_ftestRejectReason;  // FtestReject

void Communication_SendFtestStart(void);
void Communication_SendFtestRun(uint8_t id);
void Communication_SendFtestAbort(void);
void Communication_SendFtestConfirm(uint8_t id, bool ok);

void Communication_SendProfileListReq(void);
void Communication_SendProfileNew(const char *name, uint8_t gestWeeks);
void Communication_SendProfileSelect(uint32_t seq);
void Communication_SendProfileWeight(uint32_t seq, uint16_t grams); // 0 = SKIP
void Communication_SendProfileAgeManual(uint32_t seq, uint16_t ageDays);
void Communication_SendProfileDischarge(uint32_t seq, uint8_t outcome,
                                        uint8_t cause);
void Communication_SendProfileKangaroo(uint32_t seq);
void Communication_SendProfileHistoryReq(uint32_t page);
void Communication_SendWeightHistoryReq(uint32_t seq);

// ======================
//   PUBLIC FUNCTIONS
// ======================
void CreateCommTask();
TaskHandle_t CommTask_GetHandle(void);
void Communication_RequestState(void);
void Communication_UIReady(void);
void Communication_SendBootInfo(void);
void Communication_SendWiFiCredentials(const char *ssid, const char *password);
bool Display_ApplyCtrlState(const ControlBoard_Message_State &st);

#endif

#ifndef _GPRS_H_
#define _GPRS_H_

#include "main.h"

#define APN_ONOMONDO "onomondo"
#define APN_TM "TM"
#define APN_TRUPHONE "iot.truphone.com"

#define GPRS_USER ""
#define GPRS_PASS ""

// Baud rate for debug serial
#define SERIAL_DEBUG_BAUD 115200
#define MODEM_BAUD 115200
#define RX_BUFFER_LENGTH 1024
#define GPRS_TIMEOUT 30000 // in millisecs

// Los valores viven en transport_policy.h, que es la tabla única GPRS/WiFi.
// Estos alias mantienen los nombres que ya usa el código.
#include "config/transport_policy.h"

#define standByGPRSPostPeriod TX_GPRS_PERIOD_STANDBY_S
#define actuatingGPRSPostPeriod TX_GPRS_PERIOD_ACTUATING_S
#define phototherapyGPRSPostPeriod TX_GPRS_PERIOD_PHOTOTHERAPY_S
#define GPRS_SHUT OFF
#define GPRS_RECONNECT_INTERVAL TX_GPRS_RECONNECT_MS
#define GPRS_OTA_CHECK_INTERVAL TX_GPRS_OTA_CHECK_MS
#ifndef THINGSBOARD_RECONNECT_DELAY // Wifi_OTA.h define el mismo alias
#define THINGSBOARD_RECONNECT_DELAY TX_THINGSBOARD_RECONNECT_MS
#endif
// Cell-tower triangulation is a blocking AT round-trip; the incubator doesn't
// move, so it doesn't need refreshing on every telemetry send (as often as
// every 60 s in actuation mode).
#define GPRS_TRIANGULATION_INTERVAL 1800000 // 30 minutes in milliseconds
// Retry cadence for the cellular clock sync. Each attempt is a blocking AT
// round-trip (and possibly an NTP one), so back off hard: the clock only has
// to be found once per power cycle.
#define GPRS_TIME_SYNC_RETRY_INTERVAL 120000 // 2 minutes in milliseconds
// Una vez resuelta la zona horaria (por NITZ o porque IP la haya rellenado
// mientras tanto) se refresca a este ritmo en vez de darse por buena para
// siempre; ver TX_TIMEZONE_REFRESH_MS en transport_policy.h.
#define GPRS_TZ_REFRESH_INTERVAL TX_TIMEZONE_REFRESH_MS
// Cada cuanto se reverifica el adjunto de red (AT+CREG?) mientras GPRS.post
// ya esta en estado estable. Sin esto, GPRS.post se fija a true una vez (ver
// GPRSPowerUp() caso 2) y nunca se vuelve a comprobar: retirar la SIM o
// perder cobertura del todo con el modem ya enganchado no se detectaba jamas.
// 30 s: mismo orden de magnitud que otros sondeos periodicos del modem
// (GPRSUpdateCSQ()), sin generar trafico AT excesivo.
#define GPRS_ATTACH_RECHECK_INTERVAL 30000 // 30 seconds in milliseconds
// Cadencia de la traza "esperando número de serie": GPRSPost() se ejecuta cada
// pocos ms, así que sin límite la traza ahoga el resto del log.
#define GPRS_SERIAL_WAIT_LOG_PERIOD 30000 // 30 seconds in milliseconds

#define SIMCOM800_AT "AT\n"
#define SIMCOM800_ASK_CPIN "AT+CPIN?\n"
#define SIMCOM800_AT_CFUN "AT+CFUN=1\n"

#define AT_OK "OK"
#define AT_CPIN_READY "+CPIN: READY"
#define AT_CPIN_SIM_PIN "+CPIN: SIM PIN"
#define AT_ERROR "ERROR"
#define SIMCOM800_ENTER_PIN "AT+CPIN=1503\n"

#define CREDENTIALS_TYPE "credentialsType"
#define CREDENTIALS_VALUE "credentialsValue"
#define CLIENT_ID "clientId"
#define CLIENT_PASSWORD "password"
#define CLIENT_USERNAME "userName"

#define FW_STATE_UPDATED "UPDATED"

#define PROVISION_MAX_RETRIES 3

constexpr char ACCESS_TOKEN_CRED_TYPE[] PROGMEM = "ACCESS_TOKEN";
constexpr char MQTT_BASIC_CRED_TYPE[] PROGMEM = "MQTT_BASIC";

struct GPRSstruct {
  int provisioned = false;
  bool OTA_requested = false;
  bool provision_request_sent = false;
  bool provision_request_processed = false;
  bool lastGPRSConnectionStatus = false;
  bool serverConnectionStatus = false;
  bool lastServerConnectionStatus = false;
  bool OTAInProgress = false;
  bool thingsboardConnection = true;
  bool lastOTAInProgress = false;
  long lastOTACheck = false;
  long lastPpgSnapshotAttempt = false; // captura PPG automática (ver policy)
  long lastReconnectAttempt = false;
  long lastTriangulationUpdate = false;
  bool enable = false;
  long sendPeriod = false;
  long lastSent = false;
  char buffer[RX_BUFFER_LENGTH];
  int charToRead = false;
  int bufferWritePos = false;
  bool powerUp = false;
  bool connect = false;
  bool connectionStatus = false;
  bool timeOut = false;
  byte process = false;
  long processTime = false;
  long packetSentenceTime = false;
  bool post = false;
  bool firstPublish = false;
  bool firstConfigPost = false;
  bool firstPowerUp = true;
  bool pinAttempted = false; // only ever auto-enter the SIM PIN once (PUK-lock risk)

  String CCID;
  String IMEI;
  String IMSI;
  String COP;
  int CSQ;
  String APN;
  uint8_t apnIndex = 0; // index into GPRS_APN_LIST, onomondo tried first
  IPAddress IP;
  String device_token;
  uint8_t provision_retry_count = 0;

  float longitud;
  float latitud;
  float accuracy;
};

// Struct for client connecting after provisioning
struct Credentials {
  std::string client_id;
  std::string username;
  std::string password;
};

extern GPRSstruct GPRS;

bool GPRSCheckNewEvent();
bool GPRSIsAttached();
bool GPRSIsConnectedToServer();
void GPRS_Handler();
void GPRS_TB_Init();
void initGPRS();
void GPRSSetPostPeriod();
void progressCallback(const uint32_t &currentChunk,
                      const uint32_t &totalChuncks);
void updatedCallback(const bool &success);
#endif // _GPRS_123H_
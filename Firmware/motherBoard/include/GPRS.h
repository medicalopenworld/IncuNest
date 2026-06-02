#ifndef _GPRS_H_
#define _GPRS_H_

#include "main.h"

#define APN_TM "TM"
#define APN_TRUPHONE "iot.truphone.com"

#define GPRS_USER ""
#define GPRS_PASS ""

// Baud rate for debug serial
#define SERIAL_DEBUG_BAUD 115200
#define MODEM_BAUD 115200
#define RX_BUFFER_LENGTH 1024
#define GPRS_TIMEOUT 30000 // in millisecs

#define standByGPRSPostPeriod 3600
#define actuatingGPRSPostPeriod 60
#define phototherapyGPRSPostPeriod 180
#define GPRS_SHUT OFF
#define GPRS_RECONNECT_INTERVAL 10000     // 10 seconds
#define GPRS_OTA_CHECK_INTERVAL 600000    // 10 minutes in milliseconds
#define THINGSBOARD_RECONNECT_DELAY 30000 // 30 seconds

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
  long lastReconnectAttempt = false;
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

  String CCID;
  String IMEI;
  String IMSI;
  String COP;
  int CSQ;
  String APN;
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
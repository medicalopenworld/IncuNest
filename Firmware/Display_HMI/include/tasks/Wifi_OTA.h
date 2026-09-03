
#ifndef _WIFI_OTA_H_
#define _WIFI_OTA_H_

#include <Arduino.h>
#include <string>

#define THINGSBOARD_ENABLE_PSRAM 0
#define THINGSBOARD_ENABLE_DYNAMIC 1
#include <Arduino_MQTT_Client.h>
#include <ESPmDNS.h>
#include <Espressif_Updater.h>
#include <ThingsBoard.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "main.h"

#define CURRENT_FIRMWARE_TITLE "IncuNest_HMI"

#define WIFI_PUBLISH_INTERVAL 5000 // milliseconds
// Arduino-ESP32 3.x WiFi association can take >10 s on some APs; retrying
// wifiInit() before the in-flight WiFi.begin() finishes corrupts STA state
// (ESP_ERR_WIFI_CONN + HANDSHAKE_TIMEOUT) and has triggered IRQ-wdt panics.
#define WIFI_RECONNECT_INTERVAL 30000     // 30 seconds (base interval — no bajar, ver comentario arriba)
// Backoff exponencial tras fallos consecutivos de reconexión (NO_AP_FOUND
// prolongado observado en campo: bucles de 30s durante 50+ minutos seguidos
// martillean WiFi.begin() sin parar). Duplica el intervalo tras cada intento
// fallido hasta este tope; se resetea a WIFI_RECONNECT_INTERVAL en el próximo
// STA_GOT_IP y en un intento manual (ver wifiResetReconnectBackoff).
//
// El tope era 5 minutos porque se asumía que cada reintento costaba una
// escritura NVS (-> glitch LCD). Esa premisa ya no aplica: WiFi.persistent(false)
// eliminó esa escritura. Medido en campo, cada WiFi.begin() cuesta 5-20 ms y el
// panel no se entera (LCD_DIAG: worst_frame ~20.7 ms estable durante los
// reintentos). El tope de 5 min solo añadía hasta 5 minutos de desconexión con
// el AP ya disponible: el HMI se quedaba esperando y solo reconectaba al pulsar
// "Conectar" a mano. 60 s mantiene el freno para una caída larga del AP
// (60 reintentos/hora en vez de 120) sin castigar la recuperación.
#define WIFI_RECONNECT_MAX_INTERVAL 60000 // 1 minuto (tope del backoff)
#define THINGSBOARD_RECONNECT_DELAY 30000 // 30 seconds
#define WIFI_OTA_CHECK_INTERVAL 60000     // 1 minute

#define ENABLE_WIFI_OTA true // enable wifi OTA
#define ENABLE_GPRS_OTA true // enable GPRS OTA
#define THINGSBOARD_BUFFER_SIZE 4096
#define THINGSBOARD_FIELDS_AMOUNT 64
#define MAX_MESSAGE_SIZE 1024
#define THINGSBOARD_QOS false
#define TELEMETRIES_DECIMALS 2
#define FIRMWARE_FAILURE_RETRIES 12
#define FIRMWARE_PACKET_SIZE 4096
#define WAIT_FAILED_OTA_CHUNKS 10U * 1000U * 1000U

struct WIFIstruct {
  int provisioned = false;
  bool provision_request_sent = false;
  bool provision_request_processed = false;
  // Lo escribe la tarea WiFi/OTA y lo lee la de UI (WIFIIsConnectedToServer()
  // desde HelpDialog): volatile como el resto de flags cruzados de Wifi_OTA.cpp.
  volatile bool serverConnectionStatus = false;
  String device_token;
  long lastReconnectAttempt = 0;
  long lastMQTTPublish = 0;
  long lastOTACheck = 0;
  long lastWifiReconnectAttempt = 0;
};

bool WIFIIsConnectedToServer();
bool WIFIIsConnected();
void WIFI_TB_Init();
void WifiOTAHandler(void);
void WIFI_TB_OTA();
void wifiInit(void);
void wifiApplyNewCredentials(const char* ssid, const char* pass);
void wifiResetReconnectBackoff(void);
void CreateOTATask();

void progressCallback(const uint32_t &currentChunk,
                      const uint32_t &totalChuncks);
void updatedCallback(const bool &success);

#define CREDENTIALS_TYPE "credentialsType"
#define CREDENTIALS_VALUE "credentialsValue"
#define CLIENT_ID "clientId"
#define CLIENT_PASSWORD "password"
#define CLIENT_USERNAME "userName"
#define FW_STATE_UPDATED "UPDATED"
constexpr char ACCESS_TOKEN_CRED_TYPE[] PROGMEM = "ACCESS_TOKEN";
constexpr char MQTT_BASIC_CRED_TYPE[] PROGMEM = "MQTT_BASIC";

// Struct for client connecting after provisioning
struct Credentials {
  std::string client_id;
  std::string username;
  std::string password;
};

#endif // _WIFI_OTA_H_

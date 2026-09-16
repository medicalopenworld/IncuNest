#pragma once

// WiFi en modo estacion sobre esp_wifi + esp_netif + esp_event (ESP-IDF
// puro), con la forma del objeto WiFi de Arduino.
//
// Solo se implementan los 19 metodos que usa el firmware (inventariados con
// grep). Tres detalles de semantica que se conservan a proposito porque el
// codigo depende de ellos:
//
//  - persistent(false) -> esp_wifi_set_storage(WIFI_STORAGE_RAM). El firmware
//    lo llama para que WiFi.begin() NO escriba en NVS: esa escritura era la
//    que hacia parpadear el LCD del HMI (ver Wifi_OTA.h del HMI).
//  - setAutoReconnect(): como en Arduino, es esta capa quien vuelve a llamar
//    a esp_wifi_connect() al recibir STA_DISCONNECTED, no el driver.
//  - onEvent(cb, evento): los manejadores se ejecutan en el bucle de eventos
//    de IDF, igual que en Arduino corrian en su tarea de eventos; nada de lo
//    que hacen alli puede bloquear.

#include <cstdint>
#include <functional>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "platform/plat_ip.h"
#include "platform/plat_string.h"

// Estado de conexion con los valores de Arduino. El firmware solo compara
// contra WL_CONNECTED, pero se conservan los demas para los logs.
typedef enum {
  WL_NO_SHIELD = 255,
  WL_IDLE_STATUS = 0,
  WL_NO_SSID_AVAIL = 1,
  WL_SCAN_COMPLETED = 2,
  WL_CONNECTED = 3,
  WL_CONNECT_FAILED = 4,
  WL_CONNECTION_LOST = 5,
  WL_DISCONNECTED = 6,
} wl_status_t;

// Modo: Arduino usaba alias de los de IDF.
#define WIFI_OFF WIFI_MODE_NULL
#define WIFI_STA WIFI_MODE_STA
#define WIFI_AP WIFI_MODE_AP
#define WIFI_AP_STA WIFI_MODE_APSTA

// Resultado de scanComplete(), como en Arduino.
#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED (-2)

// Eventos: subconjunto de los de Arduino con los mismos nombres.
typedef enum {
  ARDUINO_EVENT_WIFI_READY = 0,
  ARDUINO_EVENT_WIFI_SCAN_DONE,
  ARDUINO_EVENT_WIFI_STA_START,
  ARDUINO_EVENT_WIFI_STA_STOP,
  ARDUINO_EVENT_WIFI_STA_CONNECTED,
  ARDUINO_EVENT_WIFI_STA_DISCONNECTED,
  ARDUINO_EVENT_WIFI_STA_GOT_IP,
  ARDUINO_EVENT_WIFI_STA_LOST_IP,
  ARDUINO_EVENT_MAX,
} WiFiEvent_t;

// Informacion del evento, con los mismos miembros que usa el codigo
// (info.wifi_sta_disconnected.reason).
typedef union {
  wifi_event_sta_connected_t wifi_sta_connected;
  wifi_event_sta_disconnected_t wifi_sta_disconnected;
  ip_event_got_ip_t got_ip;
  wifi_event_sta_scan_done_t wifi_scan_done;
} WiFiEventInfo_t;

using WiFiEventFuncCb = std::function<void(WiFiEvent_t, WiFiEventInfo_t)>;

class WiFiClass {
public:
  bool mode(wifi_mode_t m);
  wifi_mode_t getMode();

  // begin(ssid, pass) configura y conecta; begin() reconecta con lo ultimo.
  wl_status_t begin(const char *ssid, const char *pass = nullptr);
  wl_status_t begin();
  bool disconnect(bool wifioff = false);
  wl_status_t status();

  void persistent(bool persistent);
  bool setAutoReconnect(bool enable);
  bool setSleep(wifi_ps_type_t ps);
  bool setHostname(const char *hostname);

  String SSID() const;
  int8_t RSSI();
  IPAddress localIP();
  String macAddress();
  bool hostByName(const char *host, IPAddress &out);

  // Escaneo asincrono, como scanNetworks(true) en Arduino.
  int16_t scanNetworks(bool async = false);
  int16_t scanComplete();
  void scanDelete();
  String SSID(uint8_t i);
  int32_t RSSI(uint8_t i);

  // Registra un manejador; con evento = ARDUINO_EVENT_MAX se llama para todos.
  void onEvent(WiFiEventFuncCb cb, WiFiEvent_t event = ARDUINO_EVENT_MAX);

private:
  bool ensureInit();
  void dispatch(WiFiEvent_t ev, const WiFiEventInfo_t &info);
  static void onWifiEvent(void *arg, esp_event_base_t base, int32_t id, void *data);
  static void onIpEvent(void *arg, esp_event_base_t base, int32_t id, void *data);

  struct Handler {
    WiFiEventFuncCb cb;
    WiFiEvent_t event;
  };
  static constexpr int kMaxHandlers = 8;
  Handler handlers_[kMaxHandlers];
  int handler_count_ = 0;

  esp_netif_t *sta_netif_ = nullptr;
  bool inited_ = false;
  bool started_ = false;
  bool auto_reconnect_ = true;
  volatile wl_status_t status_ = WL_IDLE_STATUS;
  volatile bool scan_running_ = false;
  volatile bool scan_failed_ = false;
  wifi_ap_record_t *scan_records_ = nullptr;
  uint16_t scan_count_ = 0;
  String ssid_;
  char hostname_[33] = {};
};

extern WiFiClass WiFi;

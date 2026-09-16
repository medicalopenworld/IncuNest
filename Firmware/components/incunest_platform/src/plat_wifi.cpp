#include "platform/plat_wifi.h"

#include <cstring>

#include "esp_log.h"
#include "esp_mac.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"

static const char *TAG = "plat_wifi";

WiFiClass WiFi;

bool WiFiClass::ensureInit() {
  if (inited_) {
    return true;
  }
  // Arduino hacia todo esto dentro de WiFi.mode(). Se tolera que ya este
  // hecho (ESP_ERR_INVALID_STATE) porque otros componentes —esp-mqtt, mdns—
  // tambien piden el bucle de eventos por defecto.
  esp_err_t err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_netif_init -> %s", esp_err_to_name(err));
    return false;
  }
  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_event_loop_create_default -> %s", esp_err_to_name(err));
    return false;
  }
  sta_netif_ = esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_init -> %s", esp_err_to_name(err));
    return false;
  }
  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiClass::onWifiEvent, this);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WiFiClass::onIpEvent, this);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &WiFiClass::onIpEvent, this);
  inited_ = true;
  return true;
}

bool WiFiClass::mode(wifi_mode_t m) {
  if (!ensureInit()) {
    return false;
  }
  if (esp_wifi_set_mode(m) != ESP_OK) {
    return false;
  }
  if (m == WIFI_MODE_NULL) {
    if (started_) {
      esp_wifi_stop();
      started_ = false;
    }
    return true;
  }
  if (!started_) {
    if (hostname_[0] != '\0' && sta_netif_ != nullptr) {
      esp_netif_set_hostname(sta_netif_, hostname_);
    }
    if (esp_wifi_start() != ESP_OK) {
      return false;
    }
    started_ = true;
  }
  return true;
}

wifi_mode_t WiFiClass::getMode() {
  wifi_mode_t m = WIFI_MODE_NULL;
  if (inited_) {
    esp_wifi_get_mode(&m);
  }
  return m;
}

wl_status_t WiFiClass::begin(const char *ssid, const char *pass) {
  if (!mode(WIFI_MODE_STA)) {
    return WL_CONNECT_FAILED;
  }
  wifi_config_t cfg = {};
  strncpy(reinterpret_cast<char *>(cfg.sta.ssid), ssid ? ssid : "", sizeof(cfg.sta.ssid) - 1);
  if (pass != nullptr && pass[0] != '\0') {
    strncpy(reinterpret_cast<char *>(cfg.sta.password), pass, sizeof(cfg.sta.password) - 1);
  }
  // Igual que Arduino: con clave, se exige al menos WPA2; sin ella, abierta.
  cfg.sta.threshold.authmode = (pass && pass[0]) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
  cfg.sta.pmf_cfg.capable = true;
  cfg.sta.pmf_cfg.required = false;
  esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_set_config -> %s", esp_err_to_name(err));
    return WL_CONNECT_FAILED;
  }
  ssid_ = ssid ? ssid : "";
  return begin();
}

wl_status_t WiFiClass::begin() {
  if (!mode(WIFI_MODE_STA)) {
    return WL_CONNECT_FAILED;
  }
  // Si habia una conexion a medias, esp_wifi_connect() devuelve
  // ESP_ERR_WIFI_CONN; Arduino lo ignoraba igual y dejaba que siguiera.
  esp_err_t err = esp_wifi_connect();
  if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
    ESP_LOGW(TAG, "esp_wifi_connect -> %s", esp_err_to_name(err));
    return WL_CONNECT_FAILED;
  }
  if (status_ != WL_CONNECTED) {
    status_ = WL_DISCONNECTED;
  }
  return status_;
}

bool WiFiClass::disconnect(bool wifioff) {
  if (!inited_) {
    return false;
  }
  esp_wifi_disconnect();
  status_ = WL_DISCONNECTED;
  if (wifioff) {
    mode(WIFI_MODE_NULL);
  }
  return true;
}

wl_status_t WiFiClass::status() { return status_; }

void WiFiClass::persistent(bool persistent) {
  ensureInit();
  esp_wifi_set_storage(persistent ? WIFI_STORAGE_FLASH : WIFI_STORAGE_RAM);
}

bool WiFiClass::setAutoReconnect(bool enable) {
  auto_reconnect_ = enable;
  return true;
}

bool WiFiClass::setSleep(wifi_ps_type_t ps) {
  return ensureInit() && esp_wifi_set_ps(ps) == ESP_OK;
}

bool WiFiClass::setHostname(const char *hostname) {
  if (hostname == nullptr) {
    return false;
  }
  strncpy(hostname_, hostname, sizeof(hostname_) - 1);
  if (sta_netif_ != nullptr) {
    return esp_netif_set_hostname(sta_netif_, hostname_) == ESP_OK;
  }
  return true;
}

String WiFiClass::SSID() const {
  if (status_ != WL_CONNECTED) {
    return String();
  }
  wifi_ap_record_t ap;
  if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
    return String(reinterpret_cast<const char *>(ap.ssid));
  }
  return ssid_;
}

int8_t WiFiClass::RSSI() {
  wifi_ap_record_t ap;
  if (status_ == WL_CONNECTED && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
    return ap.rssi;
  }
  return 0;
}

IPAddress WiFiClass::localIP() {
  esp_netif_ip_info_t ip = {};
  if (sta_netif_ != nullptr && esp_netif_get_ip_info(sta_netif_, &ip) == ESP_OK) {
    return IPAddress(static_cast<uint32_t>(ip.ip.addr));
  }
  return IPAddress();
}

String WiFiClass::macAddress() {
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

bool WiFiClass::hostByName(const char *host, IPAddress &out) {
  struct addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  if (getaddrinfo(host, nullptr, &hints, &res) != 0 || res == nullptr) {
    return false;
  }
  const struct sockaddr_in *sa = reinterpret_cast<const struct sockaddr_in *>(res->ai_addr);
  out = IPAddress(static_cast<uint32_t>(sa->sin_addr.s_addr));
  freeaddrinfo(res);
  return true;
}

int16_t WiFiClass::scanNetworks(bool async) {
  if (!mode(WIFI_MODE_STA)) {
    return WIFI_SCAN_FAILED;
  }
  scanDelete();
  scan_failed_ = false;
  scan_running_ = true;
  if (esp_wifi_scan_start(nullptr, !async) != ESP_OK) {
    scan_running_ = false;
    scan_failed_ = true;
    return WIFI_SCAN_FAILED;
  }
  if (async) {
    return WIFI_SCAN_RUNNING;
  }
  // Bloqueante: el evento SCAN_DONE ya se ha procesado al volver.
  return scanComplete();
}

int16_t WiFiClass::scanComplete() {
  if (scan_running_) {
    return WIFI_SCAN_RUNNING;
  }
  if (scan_failed_) {
    return WIFI_SCAN_FAILED;
  }
  return static_cast<int16_t>(scan_count_);
}

void WiFiClass::scanDelete() {
  delete[] scan_records_;
  scan_records_ = nullptr;
  scan_count_ = 0;
}

String WiFiClass::SSID(uint8_t i) {
  if (scan_records_ == nullptr || i >= scan_count_) {
    return String();
  }
  return String(reinterpret_cast<const char *>(scan_records_[i].ssid));
}

int32_t WiFiClass::RSSI(uint8_t i) {
  if (scan_records_ == nullptr || i >= scan_count_) {
    return 0;
  }
  return scan_records_[i].rssi;
}

void WiFiClass::onEvent(WiFiEventFuncCb cb, WiFiEvent_t event) {
  if (handler_count_ >= kMaxHandlers) {
    ESP_LOGE(TAG, "sin hueco para mas manejadores de evento");
    return;
  }
  handlers_[handler_count_++] = Handler{cb, event};
}

void WiFiClass::dispatch(WiFiEvent_t ev, const WiFiEventInfo_t &info) {
  for (int i = 0; i < handler_count_; i++) {
    if (handlers_[i].event == ARDUINO_EVENT_MAX || handlers_[i].event == ev) {
      handlers_[i].cb(ev, info);
    }
  }
}

void WiFiClass::onWifiEvent(void *arg, esp_event_base_t, int32_t id, void *data) {
  WiFiClass *self = static_cast<WiFiClass *>(arg);
  WiFiEventInfo_t info = {};
  switch (id) {
  case WIFI_EVENT_STA_START:
    self->dispatch(ARDUINO_EVENT_WIFI_STA_START, info);
    break;
  case WIFI_EVENT_STA_STOP:
    self->status_ = WL_DISCONNECTED;
    self->dispatch(ARDUINO_EVENT_WIFI_STA_STOP, info);
    break;
  case WIFI_EVENT_STA_CONNECTED:
    if (data) info.wifi_sta_connected = *static_cast<wifi_event_sta_connected_t *>(data);
    self->dispatch(ARDUINO_EVENT_WIFI_STA_CONNECTED, info);
    break;
  case WIFI_EVENT_STA_DISCONNECTED: {
    if (data) info.wifi_sta_disconnected = *static_cast<wifi_event_sta_disconnected_t *>(data);
    // Como Arduino: NO_AP_FOUND -> WL_NO_SSID_AVAIL, fallo de clave ->
    // WL_CONNECT_FAILED, el resto WL_DISCONNECTED.
    const uint8_t reason = info.wifi_sta_disconnected.reason;
    if (reason == WIFI_REASON_NO_AP_FOUND) {
      self->status_ = WL_NO_SSID_AVAIL;
    } else if (reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
               reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) {
      self->status_ = WL_CONNECT_FAILED;
    } else {
      self->status_ = WL_DISCONNECTED;
    }
    self->dispatch(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, info);
    if (self->auto_reconnect_) {
      esp_wifi_connect();
    }
    break;
  }
  case WIFI_EVENT_SCAN_DONE: {
    if (data) info.wifi_scan_done = *static_cast<wifi_event_sta_scan_done_t *>(data);
    uint16_t n = 0;
    if (info.wifi_scan_done.status == 0 && esp_wifi_scan_get_ap_num(&n) == ESP_OK && n > 0) {
      self->scan_records_ = new wifi_ap_record_t[n];
      if (esp_wifi_scan_get_ap_records(&n, self->scan_records_) == ESP_OK) {
        self->scan_count_ = n;
      } else {
        self->scanDelete();
        self->scan_failed_ = true;
      }
    } else {
      esp_wifi_clear_ap_list();
      self->scan_count_ = 0;
      self->scan_failed_ = (info.wifi_scan_done.status != 0);
    }
    self->scan_running_ = false;
    self->dispatch(ARDUINO_EVENT_WIFI_SCAN_DONE, info);
    break;
  }
  default:
    break;
  }
}

void WiFiClass::onIpEvent(void *arg, esp_event_base_t, int32_t id, void *data) {
  WiFiClass *self = static_cast<WiFiClass *>(arg);
  WiFiEventInfo_t info = {};
  if (id == IP_EVENT_STA_GOT_IP) {
    if (data) info.got_ip = *static_cast<ip_event_got_ip_t *>(data);
    self->status_ = WL_CONNECTED;
    self->dispatch(ARDUINO_EVENT_WIFI_STA_GOT_IP, info);
  } else if (id == IP_EVENT_STA_LOST_IP) {
    self->status_ = WL_CONNECTION_LOST;
    self->dispatch(ARDUINO_EVENT_WIFI_STA_LOST_IP, info);
  }
}

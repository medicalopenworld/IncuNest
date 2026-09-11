#include "gprs_modem.h"

#include <cstdlib>
#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_modem_config.h"
#include "esp_modem_dce_config.h"
#include "esp_netif_defaults.h"
#include "esp_sntp.h"
#include "platform/plat_time.h"

static const char *TAG = "gprs_modem";

// --------------------------------------------------------------- ciclo de vida

bool GprsModem::begin(int uart_num, int tx_pin, int rx_pin, int baud, int rx_buffer) {
  if (dce_ != nullptr) {
    return true;
  }
  // esp_netif y el bucle de eventos los arranca ya la capa WiFi; se tolera
  // que esten hechos.
  esp_err_t err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return false;
  }
  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return false;
  }

  esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_PPP();
  ppp_netif_ = esp_netif_new(&netif_cfg);
  if (ppp_netif_ == nullptr) {
    ESP_LOGE(TAG, "no se pudo crear la interfaz PPP");
    return false;
  }
  esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, &GprsModem::onIpEvent, this);
  esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_LOST_IP, &GprsModem::onIpEvent, this);

  esp_modem_dte_config_t dte_cfg = ESP_MODEM_DTE_DEFAULT_CONFIG();
  dte_cfg.uart_config.port_num = static_cast<uart_port_t>(uart_num);
  dte_cfg.uart_config.tx_io_num = tx_pin;
  dte_cfg.uart_config.rx_io_num = rx_pin;
  dte_cfg.uart_config.rts_io_num = -1;
  dte_cfg.uart_config.cts_io_num = -1;
  dte_cfg.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;
  dte_cfg.uart_config.baud_rate = baud;
  dte_cfg.uart_config.rx_buffer_size = rx_buffer;
  dte_cfg.uart_config.tx_buffer_size = 512;
  dte_cfg.uart_config.event_queue_size = 30;
  dte_cfg.dte_buffer_size = 1024;
  // La tarea del DTE se queda por debajo de la de comunicacion con el HMI
  // (7) y de la del SensorBoard (8): el cable no debe ceder ante el modem.
  dte_cfg.task_priority = 5;
  dte_cfg.task_stack_size = 6144;

  esp_modem_dce_config_t dce_cfg = ESP_MODEM_DCE_DEFAULT_CONFIG("onomondo");
  dce_ = esp_modem_new_dev(ESP_MODEM_DCE_SIM800, &dte_cfg, &dce_cfg, ppp_netif_);
  if (dce_ == nullptr) {
    ESP_LOGE(TAG, "no se pudo crear el DCE SIM800");
    esp_netif_destroy(ppp_netif_);
    ppp_netif_ = nullptr;
    return false;
  }
  return true;
}

void GprsModem::end() {
  if (dce_ != nullptr) {
    esp_modem_destroy(dce_);
    dce_ = nullptr;
  }
  if (ppp_netif_ != nullptr) {
    esp_netif_destroy(ppp_netif_);
    ppp_netif_ = nullptr;
  }
  ppp_has_ip_ = false;
  cmux_ = false;
}

void GprsModem::onIpEvent(void *arg, esp_event_base_t, int32_t id, void *) {
  GprsModem *self = static_cast<GprsModem *>(arg);
  if (id == IP_EVENT_PPP_GOT_IP) {
    self->ppp_has_ip_ = true;
    // Red de seguridad para el reloj: con PPP hay IP de verdad, asi que el
    // reloj se puede poner por SNTP sobre lwIP sin depender del CNTP del
    // modem (que necesita el portador interno, ver la cabecera). Si el WiFi
    // ya lo habia arrancado, configTime() simplemente lo reinicia.
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    ESP_LOGI(TAG, "PPP con IP");
  } else if (id == IP_EVENT_PPP_LOST_IP) {
    self->ppp_has_ip_ = false;
    ESP_LOGW(TAG, "PPP sin IP");
  }
}

// ---------------------------------------------------------------- AT crudo

bool GprsModem::sendAT(const char *cmd, char *out, size_t out_len, int timeout_ms) {
  if (dce_ == nullptr || cmd == nullptr) {
    return false;
  }
  // esp_modem_at() escribe hasta CONFIG_ESP_MODEM_C_API_STR_MAX (128) en el
  // bufer de salida; se pasa por uno propio de ese tamano y se copia.
  char tmp[256] = {};
  // GPRS.cpp manda los comandos con "\n" al final (SIMCOM800_ASK_CPIN =
  // "AT+CPIN?\n"); esp_modem anade su propio terminador, asi que se recorta.
  char clean[64] = {};
  strncpy(clean, cmd, sizeof(clean) - 1);
  for (char *p = clean + strlen(clean); p > clean && (p[-1] == '\n' || p[-1] == '\r'); p--) {
    p[-1] = '\0';
  }
  const esp_err_t err = esp_modem_at(dce_, clean, tmp, timeout_ms);
  if (out != nullptr && out_len > 0) {
    strncpy(out, tmp, out_len - 1);
    out[out_len - 1] = '\0';
  }
  return err == ESP_OK;
}

// Ejecuta `cmd` y devuelve en `line` la linea que empieza por `prefix`
// (sin el prefijo), como hacia waitResponse(GF("+XXX:")) de TinyGSM.
bool GprsModem::atLine(const char *cmd, const char *prefix, char *line, size_t line_len,
                       int timeout_ms) {
  memset(resp_, 0, sizeof(resp_));
  esp_modem_at(dce_, cmd, resp_, timeout_ms);
  const char *p = strstr(resp_, prefix);
  if (p == nullptr) {
    return false;
  }
  p += strlen(prefix);
  while (*p == ' ') {
    p++;
  }
  size_t n = 0;
  while (p[n] != '\0' && p[n] != '\r' && p[n] != '\n' && n < line_len - 1) {
    line[n] = p[n];
    n++;
  }
  line[n] = '\0';
  return true;
}

// ------------------------------------------------------- identidad / red

String GprsModem::getSimCCID() {
  // TinyGSM: AT+CCID, primera linea, quitar "CCID:" si viene, trim. GPRS.cpp
  // le quita luego el ultimo caracter; se devuelve exactamente lo mismo que
  // devolvia TinyGSM para que ese recorte siga cayendo en el mismo sitio.
  memset(resp_, 0, sizeof(resp_));
  if (esp_modem_at(dce_, "AT+CCID", resp_, 2000) != ESP_OK && resp_[0] == '\0') {
    return String();
  }
  String res(resp_);
  res.replace("CCID:", "");
  res.replace("OK", "");
  res.trim();
  // Solo la primera linea, por si el modem devuelve mas.
  const int nl = res.indexOf('\r');
  if (nl > 0) {
    res = res.substring(0, nl);
  }
  res.trim();
  return res;
}

String GprsModem::getIMEI() {
  char buf[64] = {};
  if (esp_modem_get_imei(dce_, buf) != ESP_OK) {
    return String();
  }
  return String(buf);
}

String GprsModem::getIMSI() {
  char buf[64] = {};
  if (esp_modem_get_imsi(dce_, buf) != ESP_OK) {
    return String();
  }
  return String(buf);
}

String GprsModem::getOperator() {
  char buf[64] = {};
  int act = 0;
  if (esp_modem_get_operator_name(dce_, buf, &act) != ESP_OK) {
    return String();
  }
  return String(buf);
}

int16_t GprsModem::getSignalQuality() {
  int rssi = 99, ber = 99;
  if (esp_modem_get_signal_quality(dce_, &rssi, &ber) != ESP_OK) {
    return 99;
  }
  return static_cast<int16_t>(rssi);
}

bool GprsModem::isNetworkConnected() {
  // Mismo criterio que TinyGSM para SIM800: AT+CREG? -> "+CREG: n,<stat>" y
  // conectado si stat es 1 (casa) o 5 (roaming).
  char line[32] = {};
  if (!atLine("AT+CREG?", "+CREG:", line, sizeof(line), 2000)) {
    return false;
  }
  const char *comma = strchr(line, ',');
  if (comma == nullptr) {
    return false;
  }
  const int stat = atoi(comma + 1);
  return stat == 1 || stat == 5;
}

// ------------------------------------------------------------ datos (PPP)

bool GprsModem::openInternalBearer(const char *apn) {
  // Portador IP interno del SIM800 (SAPBR), del que dependen CLBS y CNTP. Con
  // TinyGSM lo levantaba gprsConnect(); aqui es best-effort tras PPP. Ver el
  // riesgo en la cabecera.
  char cmd[96];
  esp_modem_at(dce_, "AT+SAPBR=3,1,\"Contype\",\"GPRS\"", resp_, 2000);
  snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,\"APN\",\"%s\"", apn);
  esp_modem_at(dce_, cmd, resp_, 2000);
  const esp_err_t err = esp_modem_at(dce_, "AT+SAPBR=1,1", resp_, 85000);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "portador interno (SAPBR) no disponible con PPP: CLBS/CNTP no funcionaran");
    return false;
  }
  return true;
}

bool GprsModem::gprsConnect(const char *apn, const char *user, const char *pass) {
  (void)user; // GPRS_USER/GPRS_PASS estan vacios en todos los APN de la lista
  (void)pass;
  if (dce_ == nullptr || apn == nullptr) {
    return false;
  }
  if (cmux_) {
    // Reintento con otro APN: volver a modo comandos antes de reconfigurar.
    esp_modem_set_mode(dce_, ESP_MODEM_MODE_COMMAND);
    cmux_ = false;
    ppp_has_ip_ = false;
  }

  esp_modem_PdpContext_t pdp = {};
  pdp.context_id = 1;
  pdp.protocol_type = "IP";
  pdp.apn = apn;
  if (esp_modem_set_pdp_context(dce_, &pdp) != ESP_OK) {
    ESP_LOGW(TAG, "no se pudo fijar el APN '%s'", apn);
    return false;
  }

  // CMUX: canal de comandos + canal PPP por el mismo UART.
  if (esp_modem_set_mode(dce_, ESP_MODEM_MODE_CMUX) != ESP_OK) {
    ESP_LOGW(TAG, "no se pudo entrar en CMUX/PPP con APN '%s'", apn);
    esp_modem_set_mode(dce_, ESP_MODEM_MODE_COMMAND);
    return false;
  }
  cmux_ = true;

  // Bloquea como bloqueaba TinyGSM: GPRS.cpp refresca processTime al volver.
  const uint32_t t0 = millis();
  while (!ppp_has_ip_ && static_cast<uint32_t>(millis() - t0) < GPRS_PPP_CONNECT_TIMEOUT_MS) {
    delay_ms(100);
  }
  if (!ppp_has_ip_) {
    ESP_LOGW(TAG, "PPP sin IP tras %lu ms con APN '%s'",
             (unsigned long)GPRS_PPP_CONNECT_TIMEOUT_MS, apn);
    esp_modem_set_mode(dce_, ESP_MODEM_MODE_COMMAND);
    cmux_ = false;
    return false;
  }

  openInternalBearer(apn);
  return true;
}

bool GprsModem::gprsDisconnect() {
  if (dce_ == nullptr) {
    return false;
  }
  const esp_err_t err = esp_modem_set_mode(dce_, ESP_MODEM_MODE_COMMAND);
  cmux_ = false;
  ppp_has_ip_ = false;
  return err == ESP_OK;
}

IPAddress GprsModem::localIP() {
  esp_netif_ip_info_t ip = {};
  if (ppp_netif_ != nullptr && esp_netif_get_ip_info(ppp_netif_, &ip) == ESP_OK) {
    return IPAddress(static_cast<uint32_t>(ip.ip.addr));
  }
  return IPAddress();
}

// ------------------------------------------------------------ hora / lugar

bool GprsModem::getNetworkTime(int *year, int *month, int *day, int *hour, int *minute,
                               int *second, float *timezone) {
  // +CCLK: "yy/MM/dd,hh:mm:ss+zz"  (zz en cuartos de hora, como TinyGSM)
  char line[48] = {};
  if (!atLine("AT+CCLK?", "+CCLK:", line, sizeof(line), 2000)) {
    return false;
  }
  const char *p = strchr(line, '"');
  if (p == nullptr) {
    return false;
  }
  p++;
  int yy = 0, mo = 0, dd = 0, hh = 0, mi = 0, ss = 0, tz = 0;
  char sign = '+';
  if (sscanf(p, "%d/%d/%d,%d:%d:%d%c%d", &yy, &mo, &dd, &hh, &mi, &ss, &sign, &tz) < 6) {
    return false;
  }
  if (sign == '-') {
    tz = -tz;
  }
  if (yy < 2000) {
    yy += 2000;
  }
  if (year) *year = yy;
  if (month) *month = mo;
  if (day) *day = dd;
  if (hour) *hour = hh;
  if (minute) *minute = mi;
  if (second) *second = ss;
  if (timezone) *timezone = static_cast<float>(tz) / 4.0f;
  return true;
}

int GprsModem::NTPServerSync(const String &server, int timezone) {
  // Misma secuencia que TinyGSM: CNTPCID=1 (puede fallar), CNTP="srv",tz y
  // CNTP. El codigo llega como URC "+CNTP: <code>" DESPUES del OK, asi que se
  // espera con at_raw hasta verlo, no hasta el OK.
  esp_modem_at(dce_, "AT+CNTPCID=1", resp_, 10000);
  char cmd[96];
  snprintf(cmd, sizeof(cmd), "AT+CNTP=\"%s\",%d", server.c_str(), timezone);
  if (esp_modem_at(dce_, cmd, resp_, 10000) != ESP_OK) {
    return -1;
  }
  memset(resp_, 0, sizeof(resp_));
  if (esp_modem_at_raw(dce_, "AT+CNTP\r", resp_, "+CNTP:", "ERROR", 10000) != ESP_OK) {
    return -1;
  }
  const char *p = strstr(resp_, "+CNTP:");
  if (p == nullptr) {
    return -1;
  }
  return atoi(p + 6);
}

bool GprsModem::getGsmLocation(float *lat, float *lon, float *accuracy, int *year,
                               int *month, int *day, int *hour, int *minute, int *second) {
  // AT+CLBS=4,1 -> "+CLBS: 0,<campo1>,<campo2>,<precision>,yyyy/mm/dd,hh:mm:ss"
  // Parseo POSICIONAL identico a TinyGSM: campo1 -> lat, campo2 -> lon. Ver
  // la nota de la cabecera sobre las dos inversiones que se anulan.
  char line[96] = {};
  if (!atLine("AT+CLBS=4,1", "+CLBS:", line, sizeof(line), 120000)) {
    return false;
  }
  int code = -1, acc = 0, yy = 0, mo = 0, dd = 0, hh = 0, mi = 0, ss = 0;
  float f1 = 0.0f, f2 = 0.0f;
  const int n = sscanf(line, "%d,%f,%f,%d,%d/%d/%d,%d:%d:%d", &code, &f1, &f2, &acc, &yy,
                       &mo, &dd, &hh, &mi, &ss);
  if (n < 1 || code != 0) {
    return false;
  }
  if (lat) *lat = f1;
  if (lon) *lon = f2;
  if (accuracy) *accuracy = static_cast<float>(acc);
  if (year) *year = yy;
  if (month) *month = mo;
  if (day) *day = dd;
  if (hour) *hour = hh;
  if (minute) *minute = mi;
  if (second) *second = ss;
  return true;
}

// ------------------------------------------------------------------- SIM

bool GprsModem::readPin(bool *pin_ok) {
  return dce_ != nullptr && esp_modem_read_pin(dce_, pin_ok) == ESP_OK;
}

bool GprsModem::setPin(const char *pin) {
  return dce_ != nullptr && esp_modem_set_pin(dce_, pin) == ESP_OK;
}

void GprsModem::powerDown() {
  if (dce_ == nullptr) {
    return;
  }
  if (cmux_) {
    esp_modem_set_mode(dce_, ESP_MODEM_MODE_COMMAND);
    cmux_ = false;
    ppp_has_ip_ = false;
  }
  // El SIM800 contesta "NORMAL POWER DOWN", no OK.
  esp_modem_at_raw(dce_, "AT+CPOWD=1\r", resp_, "NORMAL POWER DOWN", "ERROR", 3000);
}

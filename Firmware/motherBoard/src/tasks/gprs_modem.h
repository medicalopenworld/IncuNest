#pragma once

// Modem celular SIM800 sobre esp_modem (ESP-IDF puro), con la forma de los
// metodos de TinyGSM que usaba GPRS.cpp.
//
// DECISION (2026-09-11): esp_modem con PPP, no un cliente AT propio. Con
// TinyGSM el TCP lo hacia el propio modem por comandos AT (CIPSTART...); con
// PPP el modem se convierte en una interfaz de red mas de lwIP y MQTT/HTTPS
// por celular van por el MISMO camino que por WiFi. Un solo camino de red en
// vez de dos.
//
// POR QUE LA FORMA DE TINYGSM: la maquina de estados de GPRS.cpp (powerUp ->
// connect -> post), sus plazos, su lista de APN y los ganchos del test de
// fabrica (GPRS.modemResponded, GPRS.simReady, GPRS.buffer con strstr) estan
// escritos contra esos nombres. Conservarlos deja la maquina de estados
// intacta y el cambio acotado a esta clase.
//
// MODO CMUX: el codigo consulta CSQ, CCLK, CLBS mientras la sesion de datos
// esta levantada. En DATA_MODE puro no se pueden mandar AT; en CMUX el modem
// multiplexa un canal de comandos y otro de datos por el mismo UART.
//
// RIESGO PENDIENTE DE BANCO: CLBS (localizacion por celda) y CNTP (NTP del
// propio modem) usan el portador IP INTERNO del SIM800 (AT+SAPBR), que es
// otra pila TCP/IP distinta de PPP. Con TinyGSM ese portador siempre estaba
// abierto porque su gprsConnect() lo levantaba. Aqui se intenta abrir
// tambien (best-effort) tras levantar PPP, pero no esta comprobado que el
// SIM800 mantenga PPP y SAPBR a la vez con una sola SIM. Si no puede:
//   - la localizacion por celda dejara de refrescarse (queda la de IP), y
//   - el reloj se pone en hora igualmente, porque al obtener IP por PPP se
//     arranca SNTP sobre lwIP (ver onPppGotIp); GPRSEnsureTimeSynced() ve el
//     reloj puesto y no necesita CNTP.
// Hay que medirlo con una SIM Onomondo real antes de tocar la tirada de 200.

#include <cstdint>

#include "esp_modem_api.h"
#include "esp_modem_c_api_types.h"
#include "esp_netif.h"
#include "platform/plat_ip.h"
#include "platform/plat_string.h"

class GprsModem {
public:
  // Crea DTE (UART), interfaz PPP y DCE SIM800 en modo comandos. No habla
  // con el modem todavia: eso lo hace la maquina de estados con sendAT().
  bool begin(int uart_num, int tx_pin, int rx_pin, int baud, int rx_buffer);
  void end();
  bool isReady() const { return dce_ != nullptr; }

  // AT crudo. `out` recibe la respuesta (lineas hasta OK/ERROR) para que el
  // strstr de GPRS.cpp y del test de fabrica sigan funcionando igual.
  bool sendAT(const char *cmd, char *out, size_t out_len, int timeout_ms = 1000);

  // --- con la forma de TinyGSM ---
  String getSimCCID();
  String getIMEI();
  String getIMSI();
  String getOperator();
  int16_t getSignalQuality(); // CSQ 0..31 (99 = desconocido), como TinyGSM
  bool isNetworkConnected();  // CREG 1 (casa) o 5 (roaming)

  // Fija el APN, levanta PPP (modo CMUX) y espera la IP. Bloquea hasta
  // GPRS_PPP_CONNECT_TIMEOUT_MS, como bloqueaba gprsConnect() de TinyGSM.
  bool gprsConnect(const char *apn, const char *user, const char *pass);
  bool gprsDisconnect();
  bool isGprsConnected() const { return ppp_has_ip_; }
  IPAddress localIP();

  // AT+CCLK? con el mismo parseo que TinyGSM (zona en cuartos de hora / 4).
  bool getNetworkTime(int *year, int *month, int *day, int *hour, int *minute,
                      int *second, float *timezone);
  // AT+CNTPCID / AT+CNTP. Devuelve el codigo de +CNTP (1 = ok) o -1.
  int NTPServerSync(const String &server, int timezone);
  // AT+CLBS=4,1. MISMO ORDEN POSICIONAL que TinyGSM: el primer campo de la
  // respuesta va al primer puntero y el segundo al segundo. GPRS.cpp lo llama
  // con (&longitud, &latitud) sobre la firma (lat, lon) de TinyGSM, y TinyGSM
  // etiquetaba al reves lo que el SIM800 devuelve (longitud primero): las dos
  // inversiones se anulan y el resultado es correcto. NO "arreglar" una sola.
  bool getGsmLocation(float *lat, float *lon, float *accuracy, int *year,
                      int *month, int *day, int *hour, int *minute, int *second);

  bool readPin(bool *pin_ok);
  bool setPin(const char *pin);
  void powerDown(); // AT+CPOWD=1

  static constexpr uint32_t GPRS_PPP_CONNECT_TIMEOUT_MS = 75000;

private:
  static void onIpEvent(void *arg, esp_event_base_t base, int32_t id, void *data);
  bool openInternalBearer(const char *apn);
  bool atLine(const char *cmd, const char *prefix, char *line, size_t line_len,
              int timeout_ms);

  esp_modem_dce_t *dce_ = nullptr;
  esp_netif_t *ppp_netif_ = nullptr;
  volatile bool ppp_has_ip_ = false;
  bool cmux_ = false;
  char resp_[256] = {};
};

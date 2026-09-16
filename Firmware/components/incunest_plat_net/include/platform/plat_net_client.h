#pragma once

// Clientes TCP y TLS con la forma de Client / WiFiClient / WiFiClientSecure
// de Arduino, sobre sockets de lwIP y esp-tls (ESP-IDF puro).
//
// Quien los usa: la activacion de SIM en el test de fabrica (HTTPS a
// Onomondo) y la subida a Drive. Escriben la peticion HTTP a mano con
// print()/printf() y leen la respuesta con readStringUntil('\n'): por eso
// heredan de Stream y no de un cliente HTTP.
//
// OJO CON setTimeout(): en la WiFiClient de Arduino va en SEGUNDOS (a
// diferencia de Stream::setTimeout, en ms), y los dos puntos de llamada del
// firmware pasan segundos — client.setTimeout(30), setTimeout(15). Se
// conserva esa unidad aqui a proposito.

#include <cstdint>

#include "esp_tls.h"
#include "platform/plat_ip.h"
#include "platform/plat_print.h"

class Client : public Stream {
public:
  virtual int connect(const char *host, uint16_t port) = 0;
  virtual int connect(IPAddress ip, uint16_t port) = 0;
  virtual uint8_t connected() = 0;
  virtual void stop() = 0;
  virtual operator bool() = 0;
};

class WiFiClient : public Client {
public:
  WiFiClient() = default;
  ~WiFiClient() override;
  WiFiClient(const WiFiClient &) = delete;
  WiFiClient &operator=(const WiFiClient &) = delete;

  int connect(const char *host, uint16_t port) override;
  int connect(IPAddress ip, uint16_t port) override;
  uint8_t connected() override;
  void stop() override;
  operator bool() override { return connected() != 0; }

  int available() override;
  int read() override;
  int peek() override;
  size_t write(uint8_t b) override;
  size_t write(const uint8_t *buf, size_t len) override;
  using Print::write;
  void flush() override {}

  // EN SEGUNDOS, como la WiFiClient de Arduino.
  void setTimeout(uint32_t seconds);

protected:
  virtual int rawRead(uint8_t *buf, size_t len);
  virtual int rawWrite(const uint8_t *buf, size_t len);
  virtual int rawAvailable();
  int openSocket(const char *host, uint16_t port);

  int fd_ = -1;
  uint32_t timeout_ms_ = 3000; // defecto de Arduino
  int peeked_ = -1;
};

class WiFiClientSecure : public WiFiClient {
public:
  WiFiClientSecure() = default;
  ~WiFiClientSecure() override;

  int connect(const char *host, uint16_t port) override;
  int connect(IPAddress ip, uint16_t port) override;
  uint8_t connected() override;
  void stop() override;

  // Sin verificar el certificado del servidor. Es lo que hacia el firmware
  // (ver ftest_sim_activation.cpp: "igual que hace la GPRS"). Requiere
  // CONFIG_ESP_TLS_INSECURE y CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY en el
  // sdkconfig de la placa que lo use.
  void setInsecure() { insecure_ = true; }
  void setCACert(const char *pem) { ca_pem_ = pem; }

protected:
  int rawRead(uint8_t *buf, size_t len) override;
  int rawWrite(const uint8_t *buf, size_t len) override;
  int rawAvailable() override;

private:
  esp_tls_t *tls_ = nullptr;
  bool insecure_ = false;
  const char *ca_pem_ = nullptr;
};

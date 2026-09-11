#pragma once

// Bus I2C maestro sobre driver/i2c_master (ESP-IDF puro).
//
// POR QUE NO SE IMITA LA API DE Wire: el patron de Arduino
// (beginTransmission / write / endTransmission) es un bufer implicito con
// estado escondido en el objeto, y en IDF 6 el driver antiguo que lo sostenia
// ya no existe. La API nueva es por transaccion y por dispositivo, que ademas
// es lo que de verdad quieren los drivers de sensor: escribir un registro y
// leer N bytes. Son ~27 puntos de llamada; se convierten a mano.
//
// Los handles de dispositivo se cachean por direccion dentro de la clase: el
// coste de i2c_master_bus_add_device() se paga una vez por chip, no en cada
// transaccion.

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"

class I2cBus {
public:
  I2cBus() = default;
  ~I2cBus();

  I2cBus(const I2cBus &) = delete;
  I2cBus &operator=(const I2cBus &) = delete;

  // port = -1 deja que el driver elija un puerto libre.
  bool begin(int sda, int scl, uint32_t freq_hz, int port = -1);
  // Sin argumentos: lo llaman librerias de terceros que dan por hecho que el
  // bus ya esta configurado (Adafruit_I2CDevice). No reconfigura nada; solo
  // dice si el bus esta listo. Quien fija pines y velocidad es initHardware().
  bool begin() { return isReady(); }
  // Dos argumentos, como el Wire.begin(sda, scl) de Arduino: 100 kHz por
  // defecto, que es lo que usaba alli.
  bool begin(int sda, int scl) { return begin(sda, scl, 100000); }
  void end();
  bool isReady() const { return bus_ != nullptr; }
  uint32_t getClock() const { return freq_hz_; }

  // Sondeo de presencia. Sustituye al endTransmission() vacio que se usaba
  // como "hay alguien en esta direccion": i2c_master_probe() mira el ACK del
  // ciclo de direccion de verdad, sin escribir nada al dispositivo.
  bool probe(uint8_t addr, int timeout_ms = 50);

  bool write(uint8_t addr, const uint8_t *data, size_t len, int timeout_ms = 100);
  bool read(uint8_t addr, uint8_t *buf, size_t len, int timeout_ms = 100);
  // Escritura + lectura con START repetido (sin soltar el bus entre medias),
  // que es lo que necesitan casi todos los sensores para "lee este registro".
  bool writeRead(uint8_t addr, const uint8_t *out, size_t out_len, uint8_t *in,
                 size_t in_len, int timeout_ms = 100);

  // Atajos habituales sobre registro de 8 bits.
  bool writeReg8(uint8_t addr, uint8_t reg, uint8_t value);
  bool readReg(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len);

  // ---------------------------------------------------------------------
  // SUPERFICIE DE COMPATIBILIDAD CON Wire — SOLO PARA LIBRERIAS VENDORIZADAS
  //
  // El codigo de la aplicacion NO debe usar esto: para eso estan write(),
  // read() y writeRead() de arriba, que son una transaccion explicita. Esta
  // parte existe unicamente para poder vendorizar las librerias de sensor de
  // terceros (SHTC3, STS3x, INA3221, TCA9555...) SIN tocar su codigo.
  //
  // El motivo es el mismo que con SPI y con String: esas librerias miden la
  // temperatura del aire y de la piel, que es lo que gobierna el calefactor.
  // Reescribir sus formulas de conversion, sus CRC y sus secuencias de
  // comando a mano seria meter riesgo clinico a cambio de nada; cambiarles
  // solo el transporte y dejar la matematica intacta es mucho mas seguro y
  // ademas permite volver a fusionar cuando upstream se mueva.
  //
  // Reproduce el modelo con estado de Arduino: beginTransmission() abre un
  // bufer, write() acumula, y endTransmission() lo envia de verdad. Un
  // endTransmission(false) pide START REPETIDO, asi que no envia todavia: se
  // guarda y el siguiente requestFrom() lo resuelve con una unica
  // transaccion de escritura+lectura, que es lo que el bus veia antes.
  void beginTransmission(uint8_t addr);
  size_t write(uint8_t data);
  size_t write(const uint8_t *data, size_t len);
  uint8_t endTransmission(bool sendStop = true);
  uint8_t requestFrom(uint8_t addr, uint8_t len, bool sendStop = true);
  int available();
  int read();
  int peek();
  void setClock(uint32_t hz) { freq_hz_ = hz; }
  // Arduino tenia setTimeOut(ms) en TwoWire. Aqui el plazo va por transaccion
  // (cada write/read lo lleva como argumento), asi que esto solo guarda el
  // valor por defecto que usaran las siguientes.
  void setTimeOut(uint16_t ms) { timeout_ms_ = ms; }
  uint16_t getTimeOut() const { return timeout_ms_; }

private:
  static constexpr int kMaxDevices = 16;

  struct DevSlot {
    uint8_t addr;
    i2c_master_dev_handle_t handle;
  };

  i2c_master_dev_handle_t deviceFor(uint8_t addr);

  static constexpr size_t kBufSize = 64;

  i2c_master_bus_handle_t bus_ = nullptr;
  uint32_t freq_hz_ = 0;
  uint16_t timeout_ms_ = 100;
  DevSlot devices_[kMaxDevices] = {};
  int device_count_ = 0;

  // Estado del modo compatibilidad Wire.
  uint8_t tx_buf_[kBufSize] = {};
  size_t tx_len_ = 0;
  uint8_t tx_addr_ = 0;
  bool tx_open_ = false;
  bool pending_restart_ = false; // hubo un endTransmission(false)
  uint8_t rx_buf_[kBufSize] = {};
  size_t rx_len_ = 0;
  size_t rx_pos_ = 0;
};

// Buses globales, con los nombres de Arduino. Mismo criterio que con SPI y
// millis(): el firmware los usa como argumento por defecto en cabeceras
// (IncuNest_humidifier.h) y en punteros que se pasan a los drivers de sensor.
// Son objetos nuestros sobre i2c_master; de Arduino solo queda el nombre.
// begin() se llama en initHardware(), como antes.
extern I2cBus Wire;   // bus principal
extern I2cBus Wire1;  // segundo bus (HW16/17: SHTC3 + STS35)

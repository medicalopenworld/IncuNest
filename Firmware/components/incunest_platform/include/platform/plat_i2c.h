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

private:
  static constexpr int kMaxDevices = 16;

  struct DevSlot {
    uint8_t addr;
    i2c_master_dev_handle_t handle;
  };

  i2c_master_dev_handle_t deviceFor(uint8_t addr);

  i2c_master_bus_handle_t bus_ = nullptr;
  uint32_t freq_hz_ = 0;
  DevSlot devices_[kMaxDevices] = {};
  int device_count_ = 0;
};

// Buses globales, con los nombres de Arduino. Mismo criterio que con SPI y
// millis(): el firmware los usa como argumento por defecto en cabeceras
// (IncuNest_humidifier.h) y en punteros que se pasan a los drivers de sensor.
// Son objetos nuestros sobre i2c_master; de Arduino solo queda el nombre.
// begin() se llama en initHardware(), como antes.
extern I2cBus Wire;   // bus principal
extern I2cBus Wire1;  // segundo bus (HW16/17: SHTC3 + STS35)

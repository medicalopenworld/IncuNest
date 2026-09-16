#pragma once

// Bus SPI maestro sobre driver/spi_master (ESP-IDF puro).
//
// Se le da la FORMA de la API SPI de Arduino —begin / beginTransaction /
// transfer / endTransaction, con SPISettings— y se publica un objeto global
// llamado SPI. El motivo es el mismo que con millis() y String: el unico
// consumidor es la libreria del AFE4490 (el frontal de SpO2), que vive en
// OTRO REPOSITORIO con otro responsable y esta pineada a un commit concreto.
// Cuanto menos se toque su codigo, mas facil es volver a fusionar cuando
// upstream se mueva, y menos riesgo se mete en una ruta de monitorizacion de
// paciente.
//
// SOBRE EL COSTE POR BYTE: Arduino resolvia SPI.transfer(uint8_t) escribiendo
// directamente los registros del periferico (spiTransferByteNL), sin
// interrupciones. Aqui se usa spi_device_polling_transmit(), que es el
// equivalente de IDF y tambien va por sondeo, sin interrupciones ni DMA: el
// coste por byte queda en el mismo orden. AUN ASI, la cadencia real de las
// muestras de PPG debe comprobarse en banco antes de dar esto por bueno; es
// una senal de monitorizacion, no un log.

#include <cstdint>

#include "driver/spi_master.h"

// Constantes de Arduino que usa el codigo del AFE4490.
#ifndef MSBFIRST
#define MSBFIRST 1
#endif
#ifndef LSBFIRST
#define LSBFIRST 0
#endif
#ifndef SPI_MODE0
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3
#endif

class SPISettings {
public:
  SPISettings() = default;
  SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
      : clock_(clock), bit_order_(bitOrder), data_mode_(dataMode) {}

  uint32_t clock() const { return clock_; }
  uint8_t bitOrder() const { return bit_order_; }
  uint8_t dataMode() const { return data_mode_; }

  bool operator==(const SPISettings &o) const {
    return clock_ == o.clock_ && bit_order_ == o.bit_order_ &&
           data_mode_ == o.data_mode_;
  }

private:
  uint32_t clock_ = 1000000;
  uint8_t bit_order_ = MSBFIRST;
  uint8_t data_mode_ = SPI_MODE0;
};

class SpiBus {
public:
  // ss = -1 significa que el chip select lo maneja el driver del dispositivo
  // por GPIO, igual que hacia el codigo con Arduino.
  bool begin(int sck, int miso, int mosi, int ss = -1);
  // Sin argumentos: lo llaman librerias de terceros que dan por hecho que el
  // bus ya esta configurado (Adafruit BusIO). No reconfigura nada.
  bool begin();
  void end();

  void beginTransaction(const SPISettings &settings);
  void endTransaction();

  uint8_t transfer(uint8_t data);
  uint16_t transfer16(uint16_t data);
  void transfer(void *buf, size_t count);

private:
  bool ensureDevice(const SPISettings &settings);

  spi_host_device_t host_ = SPI2_HOST;
  spi_device_handle_t dev_ = nullptr;
  SPISettings current_;
  bool bus_ready_ = false;
  bool in_transaction_ = false;
};

// Objeto global, como el SPI de Arduino.
extern SpiBus SPI;

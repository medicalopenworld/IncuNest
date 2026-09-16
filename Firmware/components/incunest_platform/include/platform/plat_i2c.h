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
//
// ================== EXCLUSION MUTUA: ES OBLIGATORIA ==================
// La TwoWire de Arduino tomaba un mutex en beginTransmission() y no lo soltaba
// hasta endTransmission()/requestFrom(). No era decorativo: en esta placa CADA
// BUS LO COMPARTEN VARIAS TAREAS.
//
//   Wire  -> BQ25730 (PWR_MGMT), humidificador (bucle principal),
//            INA3221/ambiente (SENSORS), bateria de fabrica (FTEST)
//   Wire1 -> SensorBoard (SB_COMM) y sensores de aire STS35/SHTC3 (SENSORS)
//
// Y la superficie de compatibilidad de abajo es una MAQUINA DE ESTADOS POR BUS
// (tx_buf_, tx_addr_, pending_restart_, rx_buf_...). Sin cerrojo, dos tareas
// que se intercalen entre beginTransmission() y endTransmission() se pisan el
// bufer: una acaba escribiendo su registro en la direccion de la otra, o
// leyendo sus bytes. Eso aqui son la temperatura del aire y de la piel, que es
// lo que gobierna el calefactor. Ademas deviceFor() muta devices_[] y
// device_count_: dos altas simultaneas con device_count_ == kMaxDevices-1
// escriben las dos en el mismo hueco y dejan el contador en kMaxDevices+1,
// que ya es corrupcion de memoria.
//
// El mutex es RECURSIVO a proposito: endTransmission()/requestFrom() llaman
// por dentro a write()/read()/probe(), que lo vuelven a tomar.

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

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

  // Toma/suelta el cerrojo del bus. lock() lo crea la primera vez: los buses
  // son objetos globales y no hay orden garantizado de constructores.
  void lock();
  void unlock();

  // RAII para las rutas de una sola transaccion.
  class Guard {
  public:
    explicit Guard(I2cBus &bus) : bus_(bus) { bus_.lock(); }
    ~Guard() { bus_.unlock(); }
    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;

  private:
    I2cBus &bus_;
  };

  struct DevSlot {
    uint8_t addr;
    i2c_master_dev_handle_t handle;
  };

  i2c_master_dev_handle_t deviceFor(uint8_t addr);

  static constexpr size_t kBufSize = 64;

  SemaphoreHandle_t lock_ = nullptr;
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
  // Tarea que tiene una toma del cerrojo VIVA hecha por la superficie de
  // compatibilidad (beginTransmission la toma; endTransmission(true) o
  // requestFrom() la sueltan).
  //
  // Existe para que una transaccion abandonada no cierre el bus para siempre.
  // Se abandona de dos maneras, y las dos estan en codigo que no es nuestro:
  // un endTransmission(false) cuyo requestFrom() nunca llega, y —esta es real,
  // viene de upstream— Adafruit_I2CDevice::write(), que hace
  // beginTransmission() y se vuelve con `return false` si el bufer se queda
  // corto, sin cerrar nada. Con la TwoWire de Arduino eso filtraba su mutex
  // igual; aqui el siguiente beginTransmission() de ESA MISMA tarea devuelve
  // la toma huerfana antes de quedarse con la suya.
  TaskHandle_t compat_owner_ = nullptr;
  uint8_t rx_buf_[kBufSize] = {};
  size_t rx_len_ = 0;
  size_t rx_pos_ = 0;
  // Dueña del contenido de rx_buf_.
  //
  // El cerrojo cubre el trafico del bus, pero el que llama DRENA el bufer
  // despues, con available()/read(), y para entonces ya esta suelto —
  // soltarlo ahi es lo que hacia Arduino, y alargarlo hasta el drenaje
  // dejaria el bus muerto para siempre si alguien pide 4 bytes y lee 2.
  // Asi que en vez de alargar el cerrojo se marca de quien son los bytes: a
  // otra tarea se le contesta "no hay nada", que es como ya trata cualquiera
  // de estos drivers una lectura fallida.
  TaskHandle_t rx_owner_ = nullptr;
};

// Buses globales, con los nombres de Arduino. Mismo criterio que con SPI y
// millis(): el firmware los usa como argumento por defecto en cabeceras
// (IncuNest_humidifier.h) y en punteros que se pasan a los drivers de sensor.
// Son objetos nuestros sobre i2c_master; de Arduino solo queda el nombre.
// begin() se llama en initHardware(), como antes.
extern I2cBus Wire;   // bus principal
extern I2cBus Wire1;  // segundo bus (HW16/17: SHTC3 + STS35)

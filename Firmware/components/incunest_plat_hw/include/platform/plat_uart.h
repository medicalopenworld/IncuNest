#pragma once

// Puerto serie sobre driver/uart (ESP-IDF puro), con la forma de la
// HardwareSerial de Arduino y los tres objetos globales Serial, Serial1 y
// Serial2 sobre UART0, UART1 y UART2.
//
// Por aqui viajan las alarmas: Serial1 en la motherBoard y Serial (UART0) en
// el HMI son el enlace entre las dos placas. La forma de la API se conserva
// para que el protocolo (~80 llamadas a print/printf/println) no cambie ni
// una linea; la implementacion es el driver de UART de IDF.
//
// LO QUE MEJORA RESPECTO A ARDUINO, Y POR QUE IMPORTA AQUI:
// el tamano del anillo de RX es un argumento de uart_driver_install(), no un
// setter que solo funciona si se llama antes del primer begin(). main.cpp del
// HMI documenta como los 256 B por defecto de Arduino hacian perder LINEAS
// ENTERAS del protocolo mientras la UI repintaba. setRxBufferSize() se
// respeta igual, pero ahora simplemente guarda el valor y begin() lo aplica:
// no hay orden de llamada que lo deje sin efecto.
//
// UART0 Y LA CONSOLA (decision del 2026-09-11): en el HMI, UART0 lleva a la
// vez el protocolo con la placa y los logs de ESP_LOG, exactamente como
// pasaba con Arduino. Se mantiene asi a proposito; el parser de la
// motherBoard ya tolera las lineas de log. Para que consola y protocolo no se
// pisen a mitad de byte, begin() sobre UART0 hace que la consola escriba a
// traves del mismo driver (uart_vfs_dev_use_driver), igual que hacia Arduino.

#include <cstddef>
#include <cstdint>

#include "driver/uart.h"
#include "platform/plat_print.h"

// Configuraciones de trama con los valores de Arduino-ESP32. El firmware
// solo usa SERIAL_8N1, pero se definen las habituales por si acaso.
#define SERIAL_5N1 0x8000010
#define SERIAL_6N1 0x8000014
#define SERIAL_7N1 0x8000018
#define SERIAL_8N1 0x800001c
#define SERIAL_5N2 0x8000030
#define SERIAL_6N2 0x8000034
#define SERIAL_7N2 0x8000038
#define SERIAL_8N2 0x800003c
#define SERIAL_8E1 0x800001e
#define SERIAL_8O1 0x800001f

class HardwareSerial : public Stream {
public:
  explicit HardwareSerial(int uart_num);
  ~HardwareSerial() override;

  HardwareSerial(const HardwareSerial &) = delete;
  HardwareSerial &operator=(const HardwareSerial &) = delete;

  // Misma firma que Arduino: begin(baud, config, rxPin, txPin). Con pines a
  // -1 se usan los de defecto del UART en el chip (UART0: RX 44, TX 43 en el
  // ESP32-S3).
  void begin(unsigned long baud, uint32_t config = SERIAL_8N1, int rxPin = -1,
             int txPin = -1);
  void end();
  bool isRunning() const { return installed_; }

  // Tamano del anillo de RX. Se puede llamar antes o despues de begin():
  // si el puerto ya esta abierto, se reinstala el driver con el nuevo tamano.
  void setRxBufferSize(size_t bytes);
  void setTxBufferSize(size_t bytes);

  int available() override;
  int read() override;
  int peek() override;
  size_t write(uint8_t b) override;
  size_t write(const uint8_t *buf, size_t len) override;
  using Print::write;

  // Espera a que el FIFO de TX se vacie (uart_wait_tx_done), como el flush()
  // de HardwareSerial.
  void flush() override;

  int uartNum() const { return uart_num_; }

private:
  void reinstall();

  int uart_num_;
  bool installed_ = false;
  size_t rx_buf_ = 1024; // Arduino usaba 256; ver la nota de plat_uart.h
  size_t tx_buf_ = 0;    // 0 = write() bloquea hasta meterlo en el FIFO
  unsigned long baud_ = 115200;
  uint32_t config_ = SERIAL_8N1;
  int rx_pin_ = -1;
  int tx_pin_ = -1;
  int peeked_ = -1;
};

extern HardwareSerial Serial;  // UART0
extern HardwareSerial Serial1; // UART1
extern HardwareSerial Serial2; // UART2

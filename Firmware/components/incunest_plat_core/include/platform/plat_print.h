#pragma once

// Print y Stream: las dos clases base que Arduino ponia debajo de todo lo que
// escribe o lee bytes (puertos serie, ficheros, clientes de red). Aqui son
// ~40 lineas propias sin nada de Arduino detras.
//
// Por que existen: el firmware llama a print()/println()/printf() sobre los
// puertos serie en ~80 sitios, el protocolo con la placa se escribe asi, y
// varias librerias vendorizadas declaran parametros de tipo Print& o Stream*.
// Tener la jerarquia una sola vez evita reimplementar la familia print() en
// cada clase (HardwareSerial, FsFile...) y mantiene la semantica identica:
// println() termina en "\r\n", print(double) usa 2 decimales, etc.

#include <cstdarg>
#include <cstddef>
#include <cstdint>

#include "platform/plat_string.h"

class Print {
public:
  virtual ~Print() = default;

  virtual size_t write(uint8_t b) = 0;
  virtual size_t write(const uint8_t *buf, size_t len);
  size_t write(const char *s);
  size_t write(const char *buf, size_t len) {
    return write(reinterpret_cast<const uint8_t *>(buf), len);
  }

  size_t print(const char *s);
  size_t print(const String &s) { return print(s.c_str()); }
  size_t print(char c) { return write(static_cast<uint8_t>(c)); }
  size_t print(int v) { return print(String(v)); }
  size_t print(unsigned int v) { return print(String(v)); }
  size_t print(long v) { return print(String(v)); }
  size_t print(unsigned long v) { return print(String(v)); }
  size_t print(long long v) { return print(String(v)); }
  size_t print(unsigned long long v) { return print(String(v)); }
  // Como Arduino: 2 decimales por defecto. Ver plat_string.h.
  size_t print(double v, int decimals = 2) {
    return print(String(v, static_cast<unsigned int>(decimals)));
  }

  size_t println();
  size_t println(const char *s) { return print(s) + println(); }
  size_t println(const String &s) { return print(s) + println(); }
  size_t println(char c) { return print(c) + println(); }
  size_t println(int v) { return print(v) + println(); }
  size_t println(unsigned int v) { return print(v) + println(); }
  size_t println(long v) { return print(v) + println(); }
  size_t println(unsigned long v) { return print(v) + println(); }
  size_t println(long long v) { return print(v) + println(); }
  size_t println(unsigned long long v) { return print(v) + println(); }
  size_t println(double v, int decimals = 2) {
    return print(v, decimals) + println();
  }

  size_t printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  size_t vprintf(const char *fmt, va_list args);

  virtual void flush() {}
};

class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;

  // Lee hasta `len` bytes o hasta que pase el plazo (por defecto 1 s, como
  // Arduino). Devuelve los bytes leidos.
  size_t readBytes(char *buf, size_t len);
  size_t readBytes(uint8_t *buf, size_t len) {
    return readBytes(reinterpret_cast<char *>(buf), len);
  }
  // Lee hasta el terminador (que se consume y no se devuelve) o hasta el
  // plazo. Cadena vacia si no llego nada.
  String readStringUntil(char terminator);

  void setTimeout(unsigned long ms) { timeout_ms_ = ms; }
  unsigned long getTimeout() const { return timeout_ms_; }

protected:
  unsigned long timeout_ms_ = 1000;
};

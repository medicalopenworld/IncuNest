#pragma once

// Cadena dinamica con la API de la String de Arduino, implementada sobre
// std::string. ESP-IDF puro: no depende de arduino-esp32 ni de WString.
//
// POR QUE EXISTE, EN VEZ DE CONVERTIR A std::string:
// son 290 usos en la motherBoard, repartidos por el protocolo serie con el
// HMI, la telemetria a ThingsBoard y los caminos de alarma. Convertirlos a
// mano seria la edicion mas grande y menos revisable de todo el porte, y la
// mayoria no son cambios de tipo sino de SEMANTICA: std::string no tiene
// toInt(), ni substring(), ni concatenacion con numeros, y sobre todo no
// formatea los double igual.
//
// EL FORMATO DE LOS DOUBLE ES LO QUE OBLIGA A ESTO.
// Arduino construia String(double) con:
//     dtostrf(value, decimalPlaces + 2, decimalPlaces, buf)
// es decir 2 DECIMALES POR DEFECTO y relleno por la izquierda hasta un ancho
// minimo de decimalPlaces+2. El firmware usa esa conversion implicita sin
// pensarla —String(in3.desiredControlTemperature)— y esas cadenas salen por
// el protocolo hacia el HMI. Un "37.5" donde antes iba "37.50", o un
// "37.500000" de std::to_string, es un cambio de protocolo silencioso.
// Aqui se reproduce con snprintf("%*.*f", dp + 2, dp, v), que tiene
// exactamente la misma semantica de ancho y precision que dtostrf.
//
// Solo se implementa la parte de la API que el firmware usa de verdad
// (inventariada con grep): c_str, length, remove, toInt, substring,
// startsWith, indexOf, trim, toFloat, toDouble, replace, y los operadores de
// concatenacion y comparacion. Si hace falta algo mas, se anade aqui.

#include <cstdint>
#include <string>

class String {
public:
  String() = default;
  String(const char *s) : s_(s ? s : "") {}
  String(const std::string &s) : s_(s) {}
  String(char c) : s_(1, c) {}

  // Enteros. Arduino admitia base, pero el firmware solo usa base 10.
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned int v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  String(long long v) : s_(std::to_string(v)) {}
  String(unsigned long long v) : s_(std::to_string(v)) {}

  // Coma flotante: 2 decimales por defecto, como Arduino. Ver la nota de
  // arriba antes de tocar esto.
  String(double v, unsigned int decimalPlaces = 2);
  String(float v, unsigned int decimalPlaces = 2)
      : String(static_cast<double>(v), decimalPlaces) {}

  const char *c_str() const { return s_.c_str(); }
  unsigned int length() const { return static_cast<unsigned int>(s_.size()); }
  bool isEmpty() const { return s_.empty(); }
  const std::string &str() const { return s_; }

  char charAt(unsigned int i) const { return i < s_.size() ? s_[i] : '\0'; }
  char operator[](unsigned int i) const { return charAt(i); }

  long toInt() const;
  float toFloat() const;
  double toDouble() const;

  String substring(unsigned int from) const;
  String substring(unsigned int from, unsigned int to) const;

  int indexOf(char c, unsigned int from = 0) const;
  int indexOf(const String &needle, unsigned int from = 0) const;
  int lastIndexOf(char c) const;

  bool startsWith(const String &prefix) const;
  bool endsWith(const String &suffix) const;

  void trim();
  void replace(const String &from, const String &to);
  void remove(unsigned int index);
  void remove(unsigned int index, unsigned int count);
  void toUpperCase();
  void toLowerCase();

  String &operator+=(const String &o) { s_ += o.s_; return *this; }
  String &operator+=(const char *o) { if (o) s_ += o; return *this; }
  String &operator+=(char c) { s_ += c; return *this; }

  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator!=(const String &o) const { return s_ != o.s_; }
  bool operator<(const String &o) const { return s_ < o.s_; }
  bool operator==(const char *o) const { return s_ == (o ? o : ""); }
  bool operator!=(const char *o) const { return !(*this == o); }

private:
  std::string s_;
};

inline String operator+(const String &a, const String &b) {
  String r(a);
  r += b;
  return r;
}
inline String operator+(const String &a, const char *b) {
  String r(a);
  r += b;
  return r;
}
inline String operator+(const char *a, const String &b) {
  String r(a);
  r += b;
  return r;
}
inline bool operator==(const char *a, const String &b) { return b == a; }
inline bool operator!=(const char *a, const String &b) { return b != a; }

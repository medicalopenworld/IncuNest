#include "platform/plat_string.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Arduino imprimia en la base pedida, en MINUSCULAS para el hexadecimal y sin
// prefijo. Se reproduce igual, porque estas cadenas salen en logs que la gente
// compara a ojo entre versiones.
static std::string base_to_string(unsigned long value, unsigned char base,
                                  bool negative) {
  if (base < 2 || base > 36) {
    base = 10;
  }
  static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  std::string out;
  if (value == 0) {
    out = "0";
  }
  while (value > 0) {
    out.insert(out.begin(), digits[value % (unsigned long)base]);
    value /= (unsigned long)base;
  }
  if (negative) {
    out.insert(out.begin(), '-');
  }
  return out;
}

String::String(int v, unsigned char base)
    : s_(base_to_string(v < 0 ? (unsigned long)(-(long)v) : (unsigned long)v,
                        base, v < 0)) {}
String::String(unsigned int v, unsigned char base)
    : s_(base_to_string(v, base, false)) {}
String::String(long v, unsigned char base)
    : s_(base_to_string(v < 0 ? (unsigned long)(-v) : (unsigned long)v, base,
                        v < 0)) {}
String::String(unsigned long v, unsigned char base)
    : s_(base_to_string(v, base, false)) {}

String::String(double v, unsigned int decimalPlaces) {
  // Equivalente exacto de lo que hacia Arduino:
  //     dtostrf(v, decimalPlaces + 2, decimalPlaces, buf)
  // El primer numero es el ANCHO MINIMO (rellena por la izquierda con
  // espacios si el resultado es mas corto) y el segundo la precision.
  // "%*.*f" tiene esa misma semantica, asi que la salida coincide caracter a
  // caracter, incluido el caso raro de decimalPlaces = 0.
  const int width = static_cast<int>(decimalPlaces) + 2;
  const int prec = static_cast<int>(decimalPlaces);

  char stack_buf[64];
  int n = snprintf(stack_buf, sizeof(stack_buf), "%*.*f", width, prec, v);
  if (n < 0) {
    s_ = "nan";
    return;
  }
  if (static_cast<size_t>(n) < sizeof(stack_buf)) {
    s_.assign(stack_buf, static_cast<size_t>(n));
    return;
  }
  // Un double enorme con muchos decimales no cabe en la pila: se reintenta
  // con el tamano exacto que pide snprintf.
  std::vector<char> heap_buf(static_cast<size_t>(n) + 1);
  snprintf(heap_buf.data(), heap_buf.size(), "%*.*f", width, prec, v);
  s_.assign(heap_buf.data(), static_cast<size_t>(n));
}

// Arduino usaba atol/atof, que NO fallan: devuelven 0 ante una cadena que no
// es un numero. Se mantiene ese comportamiento a proposito — hay sitios del
// protocolo que cuentan con recibir 0 en vez de una excepcion.
long String::toInt() const { return atol(s_.c_str()); }
float String::toFloat() const { return static_cast<float>(atof(s_.c_str())); }
double String::toDouble() const { return atof(s_.c_str()); }

String String::substring(unsigned int from) const {
  if (from >= s_.size()) {
    return String();
  }
  return String(s_.substr(from));
}

String String::substring(unsigned int from, unsigned int to) const {
  // Arduino tolera indices dados la vuelta y fuera de rango devolviendo cadena
  // vacia o recortando, en vez de lanzar. Se replica.
  if (from > to) {
    unsigned int t = from;
    from = to;
    to = t;
  }
  if (from >= s_.size()) {
    return String();
  }
  if (to > s_.size()) {
    to = static_cast<unsigned int>(s_.size());
  }
  return String(s_.substr(from, to - from));
}

int String::indexOf(char c, unsigned int from) const {
  if (from >= s_.size()) {
    return -1;
  }
  const size_t pos = s_.find(c, from);
  return pos == std::string::npos ? -1 : static_cast<int>(pos);
}

int String::indexOf(const String &needle, unsigned int from) const {
  if (from > s_.size()) {
    return -1;
  }
  const size_t pos = s_.find(needle.s_, from);
  return pos == std::string::npos ? -1 : static_cast<int>(pos);
}

int String::lastIndexOf(char c) const {
  const size_t pos = s_.rfind(c);
  return pos == std::string::npos ? -1 : static_cast<int>(pos);
}

bool String::startsWith(const String &prefix) const {
  return s_.size() >= prefix.s_.size() &&
         s_.compare(0, prefix.s_.size(), prefix.s_) == 0;
}

bool String::endsWith(const String &suffix) const {
  return s_.size() >= suffix.s_.size() &&
         s_.compare(s_.size() - suffix.s_.size(), suffix.s_.size(),
                    suffix.s_) == 0;
}

void String::trim() {
  // Arduino recorta espacios en blanco por los dos lados, usando isspace().
  size_t b = 0;
  while (b < s_.size() && isspace(static_cast<unsigned char>(s_[b]))) {
    b++;
  }
  size_t e = s_.size();
  while (e > b && isspace(static_cast<unsigned char>(s_[e - 1]))) {
    e--;
  }
  s_ = s_.substr(b, e - b);
}

void String::replace(const String &from, const String &to) {
  if (from.s_.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = s_.find(from.s_, pos)) != std::string::npos) {
    s_.replace(pos, from.s_.size(), to.s_);
    pos += to.s_.size();
  }
}

void String::remove(unsigned int index) {
  if (index < s_.size()) {
    s_.erase(index);
  }
}

void String::remove(unsigned int index, unsigned int count) {
  if (index < s_.size()) {
    s_.erase(index, count);
  }
}

void String::toUpperCase() {
  for (char &c : s_) {
    c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
  }
}

void String::toLowerCase() {
  for (char &c : s_) {
    c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  }
}

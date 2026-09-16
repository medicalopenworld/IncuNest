#pragma once

// Conversores para que ArduinoJson entienda nuestra String
// (doc["x"].as<String>(), doc["x"] = String(...)).
//
// ArduinoJson activaba estos conversores para la String de Arduino solo si
// la macro ARDUINO estaba definida (ARDUINOJSON_ENABLE_ARDUINO_STRING). Sin
// Arduino hay que aportarlos para el tipo propio; el protocolo de conversores
// personalizados de ArduinoJson 6 (>= 6.18) los localiza por ADL sobre String.
// Incluir ESTA cabecera, no ArduinoJson.h a secas, en los ficheros que
// mezclen JSON y String.

#include <ArduinoJson.h>

#include "platform/plat_string.h"

inline void convertFromJson(ArduinoJson::JsonVariantConst src, String &dst) {
  if (src.is<const char *>()) {
    const char *s = src.as<const char *>();
    dst = String(s ? s : "");
    return;
  }
  // Un numero o un booleano: Arduino los convertia a texto. Igual aqui.
  std::string out;
  serializeJson(src, out);
  dst = String(out);
}

inline bool canConvertFromJson(ArduinoJson::JsonVariantConst src, const String &) {
  return !src.isNull();
}

inline void convertToJson(const String &src, ArduinoJson::JsonVariant dst) {
  dst.set(src.c_str());
}

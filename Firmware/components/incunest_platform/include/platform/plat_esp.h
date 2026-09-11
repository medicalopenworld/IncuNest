#pragma once

// El objeto global `ESP` de Arduino, con los cuatro metodos que usa el
// firmware. Por dentro es esp_get_free_heap_size() y compania: son envoltorios
// de una linea sobre la API de ESP-IDF, no una dependencia de Arduino.
//
// Se conserva el nombre porque `ESP.getFreeHeap()` sale en logs de diagnostico
// y en el informe de soporte de las dos placas; renombrarlo no cambiaria nada
// del binario.

#include <cstdint>

class EspClass {
public:
  uint32_t getFreeHeap() const;
  uint32_t getMinFreeHeap() const;
  uint32_t getHeapSize() const;
  uint32_t getPsramSize() const;
  uint32_t getFreePsram() const;
  uint32_t getFlashChipSize() const;
  [[noreturn]] void restart() const;
};

extern EspClass ESP;

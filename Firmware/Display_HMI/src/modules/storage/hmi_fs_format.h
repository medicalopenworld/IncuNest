#pragma once
// Plan del formateo DIFERIDO de la particion de LittleFS (C1). Logica pura, sin
// Arduino ni esp_partition: la E/S la hace hmi_fs.cpp, que solo pregunta aqui
// que rango borrar y cuando.
//
// Por que existe: en campo la particion "spiffs" (~6 MB) esta virgen o lleva una
// imagen SPIFFS antigua. Montar con formateo en el acto puede borrarla entera
// de una vez y dejar a UI_Task parado decenas de segundos, sin banner ni sonido
// de alarma. Aqui se borra por rangos de 64 KB, uno por pasada y con separacion,
// y solo sin alarma activa y con el enlace a la placa vivo.

#include <stdint.h>

constexpr uint32_t HMIFS_ERASE_RANGE_BYTES = 64u * 1024u;
// Separacion minima entre dos borrados: cada rango de 64 KB para la cache de
// flash durante un tiempo, y entre uno y otro el refresco, el sonido y la
// lectura del UART tienen que poder respirar.
constexpr uint32_t HMIFS_ERASE_GAP_MS = 100;

struct HmiFsErasePlan {
  uint32_t total;  // bytes de la particion
  uint32_t next;   // siguiente offset a borrar
};

void hmifs_erase_plan_init(HmiFsErasePlan *p, uint32_t totalBytes);

// Siguiente rango (offset, longitud) y avanza el plan. false cuando ya esta
// todo borrado. El ultimo rango puede ser mas corto que HMIFS_ERASE_RANGE_BYTES.
bool hmifs_erase_plan_next(HmiFsErasePlan *p, uint32_t *offset, uint32_t *len);

bool hmifs_erase_plan_done(const HmiFsErasePlan *p);

// Cuantos rangos de HMIFS_ERASE_RANGE_BYTES caben (redondeando hacia arriba).
uint32_t hmifs_erase_plan_ranges(uint32_t totalBytes);

// true si ya puede hacerse el siguiente borrado (aritmetica de millis() segura
// ante la vuelta de 49 dias).
bool hmifs_erase_step_due(uint32_t nowMs, uint32_t lastStepMs);

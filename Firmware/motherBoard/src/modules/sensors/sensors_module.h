#pragma once

void sensors_module_init(void);
void sensors_module_update(void);

// Clasificacion de la sonda de piel por resistencia (sensors_module.cpp,
// junto a skinProbeFault() -- ver el comentario alli para el porque de
// clasificar por ohmios y no por ventana de milivoltios).
//
// Publica desde el modulo factory_test (SKIN_ADC, mb-factory-test D10 #5):
// antes de este cambio el tipo y skinProbeLastReading() no tenian ningun
// consumidor fuera de este .cpp, asi que ninguno de los dos estaba
// declarado en un header -- moverlo aqui es exponer una API que ya existia,
// no anadir comportamiento.
typedef enum {
  SKIN_PROBE_READING_OK = 0,
  SKIN_PROBE_READING_SHORT,  // R muy baja: sonda en corto o conector puenteado
  SKIN_PROBE_READING_OPEN,   // R muy alta: sonda desconectada o hilo partido
} SkinProbeReading;

SkinProbeReading skinProbeLastReading(void);

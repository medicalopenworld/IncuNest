#pragma once
// Calculo puro de las horas desde servicio y de su vencimiento. Sin NVS, sin
// `in3`, sin Arduino: la capa que toca el equipo es `service_hours.cpp`, y esta
// es la que se prueba en host (test/test_service_calc).
//
// Modelo: cada actuador tiene un contador de VIDA (horas acumuladas desde que se
// fabrico, persistido en NVS por timeTrackHandler) y una BASE (el valor del
// contador en el ultimo servicio registrado). Horas desde servicio = vida -
// base. Registrar un servicio mueve la base; el contador de vida no se toca.
#include <stdbool.h>
#include <stdint.h>

#include "service_hours_proto.h"

// vida - base, nunca negativo ni NaN. Una base corrupta (NaN o negativa) se
// ignora; si la vida es MENOR que la base el contador se ha reiniciado (NVS
// borrada, migracion) y la base ya no vale: se devuelve la vida.
float service_hours_since(float lifeHours, float baseHours);

// Horas enteras (hacia abajo) con saturacion en uint32; negativo/NaN -> 0.
uint32_t service_whole_hours(float hours);

// Vencido = horas >= umbral. Umbral 0 = aviso desactivado.
bool service_is_due(uint32_t sinceHours, uint32_t thresholdHours);

// Umbral compilado de un actuador (config/service_thresholds.h); 0 si el id no
// existe.
uint32_t service_threshold_hours(uint8_t id);

// Informe de los SVC_COUNT actuadores en orden de id. `life`, `base` y `thr`
// son arrays de SVC_COUNT. Con punteros nulos deja n = 0 (o no hace nada si
// `out` es nulo).
void service_calc_report(const float *life, const float *base,
                         const uint32_t *thr, ServiceReport *out);

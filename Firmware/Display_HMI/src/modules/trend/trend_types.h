#pragma once
// Tipos y constantes del modelo de tendencia (hmi-trend). Logica pura: sin
// LVGL, sin Arduino, sin FreeRTOS — se compila tal cual en [env:native].

#include <stddef.h>
#include <stdint.h>

// Una ranura = 10 s de reloj. El indice de un anillo ES el tiempo.
constexpr int TREND_SLOT_SECONDS = 10;
constexpr int64_t TREND_SLOT_MS = (int64_t)TREND_SLOT_SECONDS * 1000;

// Capacidad del anillo del firmware: 24 h de ranuras de 10 s por serie.
constexpr int TREND_CAPACITY = 8640;

// Series del modelo, todas en decimas (36.7 C -> 367; 55 %RH -> 550). El
// grafico ya redondeaba a decimas (Y_SCALE = 10), asi que int16 no pierde
// nada visible y el fichero comparte representacion con el anillo.
enum TrendSeries : int {
  TREND_AIR = 0,
  TREND_SKIN = 1,
  TREND_HUM = 2,
  TREND_SP_AIR = 3,
  TREND_SP_SKIN = 4,
  TREND_SP_HUM = 5,
  TREND_SERIES = 6,
};

// "Sin dato": medida no disponible (centinela PROTO_TEL_*), enlace caido,
// ranura sin telemetria o consigna que no gobierna nada. Nunca se guarda ni se
// pinta el centinela crudo de la placa como si fuera una lectura.
constexpr int16_t TREND_NO_DATA = INT16_MIN;

struct TrendSample {
  int16_t v[TREND_SERIES];
};

// Muestra con las seis series en "sin dato".
TrendSample trend_empty_sample(void);

// Valor -> decimas con redondeo al mas cercano. TREND_NO_DATA si !ok o NaN.
// Satura a [-32767, 32767] para no colisionar nunca con TREND_NO_DATA.
int16_t trend_tenths(float value, bool ok);

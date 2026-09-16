#include "time_source.h"

static Proto_TimeSource s_origin = PROTO_TIME_SOURCE_NONE;
static uint32_t s_epoch = 0;

void time_source_reset(void) {
  s_origin = PROTO_TIME_SOURCE_NONE;
  s_epoch = 0;
}

// Mayor o igual, no mayor a secas: ver la nota sobre rangos iguales en el
// header. Y funciona para el arranque sin ningun caso especial, porque
// NONE es 0 y cualquier fuente real es mayor.
bool time_source_accepts(Proto_TimeSource src) { return src >= s_origin; }

static bool epoch_in_window(uint32_t epoch) {
  return epoch >= TIME_SOURCE_MIN_EPOCH && epoch < TIME_SOURCE_MAX_EPOCH;
}

bool time_source_set(uint32_t epoch, Proto_TimeSource src) {
  // La ventana se comprueba ANTES que el rango a proposito: un epoch
  // imposible no debe consumir la prioridad de la fuente. Si un NTP entrega
  // una vez un epoch corrupto, el siguiente NTP bueno tiene que poder entrar.
  if (!epoch_in_window(epoch)) {
    return false;
  }
  if (!time_source_accepts(src)) {
    return false;
  }
  s_epoch = epoch;
  s_origin = src;
  return true;
}

Proto_TimeSource time_source_origin(void) { return s_origin; }

uint32_t time_source_epoch(void) { return s_epoch; }

bool time_source_known(void) { return s_origin != PROTO_TIME_SOURCE_NONE; }

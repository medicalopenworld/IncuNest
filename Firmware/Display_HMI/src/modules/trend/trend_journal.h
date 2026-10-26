#pragma once
// Diario de la tendencia (/trend.jnl): registros que EXTIENDEN la instantanea
// /trend.bin sin reescribirla. Logica pura sobre buffers; la E/S es de
// trend_persist + hmi_fs.
//
// Por que existe: reescribir la instantanea (~50 KB, 13 bloques) cada 5 min
// paraba la cache de flash 13 veces seguidas y el panel RGB temblaba ~1 s
// (LCD screen drift, ver lcd_recover.h). Con el diario, cada 5 min se ANADEN
// las ranuras nuevas (~30 registros, < 1 KB, una sola escritura) y la
// instantanea solo se reescribe al compactar (diario lleno, ~24 h) o cuando
// lo guardado ya no encaja (cambio de hora, fallo, sin instantanea).
//
// Registro de 24 B, little-endian explicito, byte a byte:
//   off tam campo
//     0   1  tipo: 1 = muestra, 2 = flanco de alarma
//     1   1  id de alarma        (flanco; 0 en muestra)
//     2   1  prioridad           (flanco; 0 en muestra)
//     3   1  1 = alta, 0 = baja  (flanco; 0 en muestra)
//     4   4  epoch u32 = ranura * 10 (UTC s del inicio de la ranura)
//     8  12  seis series int16 (muestra; 0 en flanco)
//    20   4  CRC-32/IEEE de los bytes 0..19
//
// Un registro con CRC o tipo invalido CORTA el diario: lo que le sigue se
// ignora (nunca se aplica un diario con huecos).

#include "modules/trend/trend_events.h"
#include "modules/trend/trend_store.h"
#include "modules/trend/trend_types.h"

constexpr size_t TREND_JNL_REC_BYTES = 24;
// Compactacion: con este numero de registros en el diario (~24 h de ranuras de
// 10 s, ~207 KB) el siguiente volcado reescribe la instantanea y lo vacia.
constexpr int TREND_JNL_MAX_RECORDS = TREND_CAPACITY;
// Un anadido mayor que esto (p. ej. tras horas sin poder escribir por alarmas)
// ya no es "una escritura pequena": se hace una instantanea completa.
constexpr int TREND_JNL_APPEND_MAX_RECORDS = 170;  // 170 * 24 = 4080 B < 4 KB

// Registros que harian falta para anadir las ranuras (afterSlot, lastSlot] y
// los flancos con slot > afterSlot.
int trend_jnl_pending_records(const TrendStore *s, const TrendEvents *e,
                              int64_t afterSlot);

// Escribe en `out` esos registros (muestras primero, luego flancos, ambos en
// orden cronologico). Devuelve los bytes escritos, o 0 si no hay nada, no cabe
// en `cap` o alguna ranura no es representable como epoch u32.
size_t trend_jnl_build(const TrendStore *s, const TrendEvents *e, int64_t afterSlot,
                       uint8_t *out, size_t cap);

// Ranura de la ULTIMA muestra valida del diario, buscando desde el final (un
// registro final roto se salta). false si no hay ninguna.
bool trend_jnl_last_sample_slot(const uint8_t *buf, size_t len, int64_t *slot);

// Aplicacion repartida en pasadas sobre un anillo y unos eventos que ya
// contienen la instantanea. `baseSlot` = ultima ranura de la instantanea: solo
// entran los flancos POSTERIORES (los anteriores ya estan en ella; un diario
// que sobrevivio a una compactacion no los duplica). Las muestras con
// slot <= lastSlot del anillo se ignoran por la misma razon.
struct TrendJnlApply {
  TrendStore *s;
  TrendEvents *e;
  const uint8_t *buf;
  size_t len;
  size_t off;
  int64_t baseSlot;
  int applied;   // registros validos recorridos
  bool broken;   // se encontro un registro invalido (el resto se ignoro)
};

void trend_jnl_apply_begin(TrendJnlApply *a, TrendStore *s, TrendEvents *e,
                           const uint8_t *buf, size_t len, int64_t baseSlot);
// Recorre como mucho `maxRecords`. MORE mientras quede; DONE al terminar (o al
// cortar en un registro invalido: ver `broken`).
TrendJobStatus trend_jnl_apply_step(TrendJnlApply *a, int maxRecords);

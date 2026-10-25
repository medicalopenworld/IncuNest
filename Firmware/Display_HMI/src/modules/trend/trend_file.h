#pragma once
// Formato de fichero de la tendencia (/trend.bin) y restauracion alineada por
// epoch. Logica pura: trabaja sobre buffers, el acceso a LittleFS es de
// hmi_fs. Little-endian explicito, byte a byte: sin struct empaquetado, para
// que host y ESP32 coincidan sin suposiciones de alineacion ni endianness.
//
//  off  tam  campo
//    0    4  magic "ITRN"
//    4    2  version = 1
//    6    2  headerSize = 32
//    8    4  slotSeconds = 10
//   12    4  count          (ranuras guardadas)
//   16    4  lastEpoch      (UTC s del inicio de la ultima ranura = lastSlot*10)
//   20    2  series = 6
//   22    2  eventCount
//   24    4  payloadCrc32   (CRC-32/IEEE de todo lo que sigue a la cabecera)
//   28    4  headerCrc32    (CRC-32/IEEE de los bytes 0..27)
//   32  6*count*2  series planar int16 LE, de la mas antigua a la mas reciente
//    .  eventCount*8  { u32 epoch, u8 id, u8 prioridad, u8 alta(1)/baja(0), u8 0 }
//
// Una version, longitud o CRC que no cuadran hacen que el fichero se ignore
// entero: nunca se restaura a medias.

#include "modules/trend/trend_events.h"
#include "modules/trend/trend_store.h"
#include "modules/trend/trend_types.h"

constexpr size_t TREND_FILE_HEADER_BYTES = 32;
constexpr size_t TREND_FILE_EVENT_BYTES = 8;
constexpr uint16_t TREND_FILE_VERSION = 1;

// Tamano maximo de un fichero para un anillo de `capacity` ranuras.
size_t trend_file_max_bytes(int capacity);

// CRC-32/IEEE (poly 0xEDB88320 reflejado, init/xorout 0xFFFFFFFF).
uint32_t trend_crc32(const uint8_t *data, size_t len);

// Serializa el anillo y los eventos que caen dentro de su rango. Devuelve los
// bytes escritos, o 0 si no hay nada que guardar (anillo vacio), no cabe en
// `cap`, o la ranura no es representable como epoch u32 (negativa / > 2106):
// el volcado solo se hace con el reloj sincronizado.
size_t trend_file_serialize(const TrendStore *s, const TrendEvents *e,
                            uint8_t *out, size_t cap);

enum TrendFileStatus : int {
  TREND_FILE_OK = 0,
  TREND_FILE_BAD_LENGTH,   // mas corto que la cabecera, o tamano que no cuadra
  TREND_FILE_BAD_MAGIC,
  TREND_FILE_BAD_HEADER,   // headerSize/slotSeconds/series/count/eventCount absurdos
  TREND_FILE_BAD_VERSION,
  TREND_FILE_BAD_CRC,      // de la cabecera o de la carga
};

struct TrendFileInfo {
  int count;
  int64_t lastSlot;   // lastEpoch / 10
  int eventCount;
};

// Valida cabecera, longitud exacta y los dos CRC. No modifica nada.
TrendFileStatus trend_file_validate(const uint8_t *buf, size_t len,
                                    TrendFileInfo *info);

enum TrendRestoreResult : int {
  TREND_RESTORE_OK = 0,
  TREND_RESTORE_INVALID,  // validacion fallida: nada cambia
  TREND_RESTORE_FUTURE,   // ultima ranura posterior a nowSlot+1: hora errada, nada cambia
  TREND_RESTORE_STALE,    // el fichero ya no cae en el anillo: nada cambia
};

// Restaura el fichero ANTEPONIENDOLO a lo ya registrado:
//  - alinea por la ranura de epoch de su ultima muestra;
//  - el hueco entre ella y la primera muestra viva queda "sin dato";
//  - donde se solapan, mandan las muestras vivas;
//  - lo que no cabe en el anillo se descarta por el lado antiguo;
//  - los eventos del fichero con ranura dentro del rango restaurado y anterior
//    al primer evento vivo se anteponen a los vivos.
// `restoredSlots` (opcional) recibe cuantas ranuras del fichero se incorporaron.
TrendRestoreResult trend_file_restore(const uint8_t *buf, size_t len,
                                      int64_t nowSlot, TrendStore *s,
                                      TrendEvents *e, int *restoredSlots);

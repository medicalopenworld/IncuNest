#include "rtc_store.h"

// Husos civiles reales, en cuartos de hora: UTC-12:00 .. UTC+14:00. Mismos
// limites que tz_source en la motherBoard.
#define TZ_Q_MIN (-48)
#define TZ_Q_MAX (56)
// Desplazamiento a sin signo para que quepa en un byte sin depender de como
// represente el compilador los negativos.
#define TZ_Q_BIAS 48

// Marca de "hay algo escrito aqui". Sin ella, una terna perfectamente legitima
// —huso UTC-12, origen desconocido, fuente desconocida— se empaquetaria como 0
// y seria indistinguible de una NVS virgen.
#define RTC_STORE_MAGIC (1u << 16)

uint32_t rtc_store_pack(const RtcStoredTz *tz) {
  if (tz == nullptr) {
    return RTC_STORE_EMPTY;
  }
  const uint32_t q = (uint32_t)((int)tz->tzQuarters + TZ_Q_BIAS) & 0xFFu;
  const uint32_t s = ((uint32_t)tz->tzSource & 0x0Fu) << 8;
  const uint32_t r = ((uint32_t)tz->src & 0x0Fu) << 12;
  return RTC_STORE_MAGIC | r | s | q;
}

bool rtc_store_unpack(uint32_t word, RtcStoredTz *out) {
  if (out == nullptr) {
    return false;
  }
  // Estado "no se sabe" por defecto: lo que se quiere ante cualquier duda.
  out->tzQuarters = 0;
  out->tzSource = 0;
  out->src = PROTO_TIME_SOURCE_NONE;

  if ((word & RTC_STORE_MAGIC) == 0) {
    return false; // NVS virgen
  }

  const int q = (int)(word & 0xFFu) - TZ_Q_BIAS;
  const uint8_t s = (uint8_t)((word >> 8) & 0x0Fu);
  const uint8_t r = (uint8_t)((word >> 12) & 0x0Fu);

  if (q < TZ_Q_MIN || q > TZ_Q_MAX) {
    return false;
  }
  if (s > 3) {
    return false; // fuera de TzSource
  }
  if (r > PROTO_TIME_SOURCE_MANUAL) {
    return false; // fuera de Proto_TimeSource
  }

  out->tzQuarters = (int8_t)q;
  out->tzSource = s;
  out->src = (Proto_TimeSource)r;
  return true;
}

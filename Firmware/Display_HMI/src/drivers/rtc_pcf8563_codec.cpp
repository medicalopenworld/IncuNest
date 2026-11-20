#include "rtc_pcf8563_codec.h"

#include "civil_time.h"

// Ventana valida, la misma de civil_to_unix_utc() y del arbitro de la
// motherBoard. Repetida aqui como constante local porque el HMI no enlaza el
// modulo time_source de la otra placa.
static const uint32_t kMinEpoch = 1609459200u; // 2021-01-01T00:00:00Z
static const uint32_t kMaxEpoch = 4102444800u; // 2100-01-01T00:00:00Z

// Un byte BCD valido tiene los dos nibbles en [0,9]. Un bus con ruido devuelve
// 0xFF, y 0xFF "convertido" a la brava da 165, que pasaria un rango de minutos
// comprobado a ojo. Por eso se valida el nibble, no el resultado.
static bool bcd_to_dec(uint8_t bcd, unsigned *out) {
  const uint8_t hi = (uint8_t)(bcd >> 4);
  const uint8_t lo = (uint8_t)(bcd & 0x0F);
  if (hi > 9 || lo > 9) {
    return false;
  }
  *out = (unsigned)(hi * 10 + lo);
  return true;
}

static uint8_t dec_to_bcd(unsigned v) {
  return (uint8_t)(((v / 10) << 4) | (v % 10));
}

bool pcf8563_decode_time(const uint8_t regs[PCF8563_TIME_REG_COUNT],
                         uint32_t *outEpoch) {
  if (regs == nullptr || outEpoch == nullptr) {
    return false;
  }

  // VL primero: si esta puesto, lo demas da igual. El chip sigue contando y
  // devuelve digitos con buena pinta, pero no significan nada.
  if ((regs[0] & 0x80) != 0) {
    return false;
  }

  unsigned second = 0, minute = 0, hour = 0, day = 0, month = 0, year2 = 0;
  if (!bcd_to_dec((uint8_t)(regs[0] & 0x7F), &second) ||
      !bcd_to_dec((uint8_t)(regs[1] & 0x7F), &minute) ||
      !bcd_to_dec((uint8_t)(regs[2] & 0x3F), &hour) ||
      !bcd_to_dec((uint8_t)(regs[3] & 0x3F), &day) ||
      !bcd_to_dec((uint8_t)(regs[5] & 0x1F), &month) ||
      !bcd_to_dec(regs[6], &year2)) {
    return false;
  }

  // El dia de la semana (regs[4]) no se usa: es redundante con la fecha y el
  // chip no lo mantiene coherente si alguien escribe solo la fecha. Se calcula
  // al escribir y se ignora al leer.

  // 2000+YY, la contrapartida de escribir el bit de siglo siempre a 0.
  const int year = 2000 + (int)year2;

  // civil_to_unix_utc valida ya los rangos civiles (mes 1-12, dia 1-31,
  // hora 0-23...) y la ventana, y devuelve false ante cualquiera de los dos.
  // Lo que no puede validar es un 31 de febrero: eso pasa el rango y produce
  // un epoch desplazado. Se comprueba aparte reconvirtiendo.
  uint32_t epoch = 0;
  if (!civil_to_unix_utc(year, month, day, hour, minute, second, 0, &epoch)) {
    return false;
  }
  if (epoch < kMinEpoch || epoch >= kMaxEpoch) {
    return false;
  }

  // Ida y vuelta: si la fecha no existe (31 de febrero, 30 de febrero de un
  // bisiesto...) el epoch cae en marzo y al deshacerlo no sale el mismo dia.
  int rtYear = 0;
  unsigned rtMonth = 0, rtDay = 0;
  civil_from_unix_utc(epoch, &rtYear, &rtMonth, &rtDay, nullptr, nullptr,
                      nullptr, nullptr);
  if (rtYear != year || rtMonth != month || rtDay != day) {
    return false;
  }

  *outEpoch = epoch;
  return true;
}

bool pcf8563_encode_time(uint32_t epoch, uint8_t regs[PCF8563_TIME_REG_COUNT]) {
  if (regs == nullptr) {
    return false;
  }
  // 2100 en adelante no cabe en la convencion de dos digitos con el bit de
  // siglo a 0. Se rechaza AQUI, antes de tocar el chip: escribirlo y
  // descubrirlo al releer dejaria el RTC con una fecha de 2000.
  if (epoch < kMinEpoch || epoch >= kMaxEpoch) {
    return false;
  }

  int year = 0;
  unsigned month = 0, day = 0, hour = 0, minute = 0, second = 0, weekday = 0;
  civil_from_unix_utc(epoch, &year, &month, &day, &hour, &minute, &second,
                      &weekday);

  regs[0] = dec_to_bcd(second);            // VL a 0: escribir declara valido
  regs[1] = dec_to_bcd(minute);
  regs[2] = dec_to_bcd(hour);
  regs[3] = dec_to_bcd(day);
  regs[4] = (uint8_t)(weekday & 0x07);     // 0 = domingo, como el chip
  regs[5] = dec_to_bcd(month);             // bit de siglo a 0, siempre
  regs[6] = dec_to_bcd((unsigned)(year - 2000));
  return true;
}

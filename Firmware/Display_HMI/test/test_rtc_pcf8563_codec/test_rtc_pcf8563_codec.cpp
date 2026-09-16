#include <unity.h>

#include "drivers/rtc_pcf8563_codec.h"

void setUp(void) {}
void tearDown(void) {}

// 2026-09-16T14:30:45Z, un miercoles. Los registros tal cual los devolveria el
// chip: BCD, VL a 0, bit de siglo a 0.
static const uint8_t kGoodRegs[PCF8563_TIME_REG_COUNT] = {
    0x45, // segundos 45, VL=0
    0x30, // minutos 30
    0x14, // horas 14
    0x16, // dia 16
    0x03, // miercoles
    0x09, // mes 9, siglo=0
    0x26, // año 26 -> 2026
};
static const uint32_t kGoodEpoch = 1789569045u;

// --- Lectura --------------------------------------------------------------

void test_decodes_valid_registers(void) {
  uint32_t epoch = 0;
  TEST_ASSERT_TRUE(pcf8563_decode_time(kGoodRegs, &epoch));
  TEST_ASSERT_EQUAL_UINT32(kGoodEpoch, epoch);
}

// El caso que da sentido a todo el filtro: con la pila agotada el chip pone
// VL y sigue contando, devolviendo digitos con buena pinta que no significan
// nada. Aqui los digitos son perfectamente validos y aun asi hay que
// rechazarlos.
void test_rejects_voltage_low_flag(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT];
  for (int i = 0; i < PCF8563_TIME_REG_COUNT; i++) regs[i] = kGoodRegs[i];
  regs[0] |= 0x80; // VL
  uint32_t epoch = 0xDEADBEEFu;
  TEST_ASSERT_FALSE(pcf8563_decode_time(regs, &epoch));
  TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, epoch); // no toca la salida
}

// Lo que devuelve un bus con ruido o un chip ausente. 0xFF "convertido" a la
// brava da 165, que pasaria un rango de minutos comprobado a ojo.
void test_rejects_all_ones(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT];
  for (int i = 0; i < PCF8563_TIME_REG_COUNT; i++) regs[i] = 0xFF;
  uint32_t epoch = 0;
  TEST_ASSERT_FALSE(pcf8563_decode_time(regs, &epoch));
}

void test_rejects_invalid_bcd_nibble(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT];
  for (int i = 0; i < PCF8563_TIME_REG_COUNT; i++) regs[i] = kGoodRegs[i];
  regs[1] = 0x1A; // nibble bajo = 10, no es BCD
  uint32_t epoch = 0;
  TEST_ASSERT_FALSE(pcf8563_decode_time(regs, &epoch));
}

void test_rejects_impossible_month(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT];
  for (int i = 0; i < PCF8563_TIME_REG_COUNT; i++) regs[i] = kGoodRegs[i];
  regs[5] = 0x13; // mes 13
  uint32_t epoch = 0;
  TEST_ASSERT_FALSE(pcf8563_decode_time(regs, &epoch));
}

// Un 31 de febrero pasa cualquier comprobacion de rango campo a campo (mes
// 1-12, dia 1-31) y produce un epoch que cae en marzo. Solo la ida y vuelta
// lo caza.
void test_rejects_nonexistent_date(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT];
  for (int i = 0; i < PCF8563_TIME_REG_COUNT; i++) regs[i] = kGoodRegs[i];
  regs[3] = 0x1F; // dia 31
  regs[5] = 0x02; // febrero
  uint32_t epoch = 0;
  TEST_ASSERT_FALSE(pcf8563_decode_time(regs, &epoch));
}

// 2025 no es bisiesto; 2024 si.
void test_rejects_feb_29_in_non_leap_year(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT];
  for (int i = 0; i < PCF8563_TIME_REG_COUNT; i++) regs[i] = kGoodRegs[i];
  regs[3] = 0x29; // dia 29
  regs[5] = 0x02; // febrero
  regs[6] = 0x25; // 2025
  uint32_t epoch = 0;
  TEST_ASSERT_FALSE(pcf8563_decode_time(regs, &epoch));

  regs[6] = 0x24; // 2024, bisiesto
  TEST_ASSERT_TRUE(pcf8563_decode_time(regs, &epoch));
}

// Un chip recien alimentado marca 2000-01-01, que queda por debajo del suelo.
void test_rejects_year_2000_default(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT] = {0x00, 0x00, 0x00, 0x01,
                                          0x06, 0x01, 0x00};
  uint32_t epoch = 0;
  TEST_ASSERT_FALSE(pcf8563_decode_time(regs, &epoch));
}

// Los bits altos que no son de dato deben ignorarse, no colarse en el valor.
void test_ignores_unused_high_bits(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT];
  for (int i = 0; i < PCF8563_TIME_REG_COUNT; i++) regs[i] = kGoodRegs[i];
  regs[1] |= 0x80; // bit7 de minutos, sin uso
  regs[2] |= 0xC0; // bits7-6 de horas, sin uso
  regs[3] |= 0xC0; // bits7-6 de dia, sin uso
  regs[5] |= 0x80; // BIT DE SIGLO: se ignora al leer, por convencion
  uint32_t epoch = 0;
  TEST_ASSERT_TRUE(pcf8563_decode_time(regs, &epoch));
  TEST_ASSERT_EQUAL_UINT32(kGoodEpoch, epoch);
}

void test_rejects_null_arguments(void) {
  uint32_t epoch = 0;
  TEST_ASSERT_FALSE(pcf8563_decode_time(nullptr, &epoch));
  TEST_ASSERT_FALSE(pcf8563_decode_time(kGoodRegs, nullptr));
}

// --- Escritura ------------------------------------------------------------

void test_encodes_valid_epoch(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT] = {0};
  TEST_ASSERT_TRUE(pcf8563_encode_time(kGoodEpoch, regs));
  TEST_ASSERT_EQUAL_HEX8(0x45, regs[0]);
  TEST_ASSERT_EQUAL_HEX8(0x30, regs[1]);
  TEST_ASSERT_EQUAL_HEX8(0x14, regs[2]);
  TEST_ASSERT_EQUAL_HEX8(0x16, regs[3]);
  TEST_ASSERT_EQUAL_HEX8(0x03, regs[4]); // miercoles
  TEST_ASSERT_EQUAL_HEX8(0x09, regs[5]);
  TEST_ASSERT_EQUAL_HEX8(0x26, regs[6]);
}

// Escribir la hora es lo que la declara valida: el VL tiene que salir a 0 o la
// siguiente lectura rechazaria lo que acabamos de escribir.
void test_encode_clears_voltage_low(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT] = {0};
  TEST_ASSERT_TRUE(pcf8563_encode_time(kGoodEpoch, regs));
  TEST_ASSERT_EQUAL_HEX8(0x00, regs[0] & 0x80);
}

// La convencion de siglo: bit a 0 SIEMPRE, año como 2000+YY.
void test_encode_always_clears_century_bit(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT] = {0};
  TEST_ASSERT_TRUE(pcf8563_encode_time(1609459200u, regs)); // 2021-01-01
  TEST_ASSERT_EQUAL_HEX8(0x00, regs[5] & 0x80);
  TEST_ASSERT_EQUAL_HEX8(0x21, regs[6]);

  TEST_ASSERT_TRUE(pcf8563_encode_time(4102444799u, regs)); // 2099-12-31
  TEST_ASSERT_EQUAL_HEX8(0x00, regs[5] & 0x80);
  TEST_ASSERT_EQUAL_HEX8(0x99, regs[6]);
}

// 2100 no es representable con esta convencion. Hay que rechazarlo ANTES de
// escribir el chip: escribirlo y descubrirlo al releer dejaria el RTC con una
// fecha del año 2000.
void test_encode_rejects_out_of_window(void) {
  uint8_t regs[PCF8563_TIME_REG_COUNT];
  for (int i = 0; i < PCF8563_TIME_REG_COUNT; i++) regs[i] = 0xAA;
  TEST_ASSERT_FALSE(pcf8563_encode_time(4102444800u, regs)); // 2100-01-01
  TEST_ASSERT_FALSE(pcf8563_encode_time(0, regs));
  TEST_ASSERT_FALSE(pcf8563_encode_time(1609459199u, regs));
  for (int i = 0; i < PCF8563_TIME_REG_COUNT; i++) {
    TEST_ASSERT_EQUAL_HEX8(0xAA, regs[i]); // no toca la salida
  }
}

// --- Ida y vuelta ---------------------------------------------------------

static void roundtrip(uint32_t epoch) {
  uint8_t regs[PCF8563_TIME_REG_COUNT] = {0};
  TEST_ASSERT_TRUE(pcf8563_encode_time(epoch, regs));
  uint32_t back = 0;
  TEST_ASSERT_TRUE(pcf8563_decode_time(regs, &back));
  TEST_ASSERT_EQUAL_UINT32(epoch, back);
}

void test_roundtrips(void) {
  roundtrip(1609459200u); // 2021-01-01, suelo
  roundtrip(kGoodEpoch);
  roundtrip(1709208000u); // 2024-02-29, bisiesto
  roundtrip(2147483648u); // 2038-01-19, el desbordamiento de 32 bits
  roundtrip(4102444799u); // 2099-12-31T23:59:59Z, techo
}

// Un dia entero hora a hora: caza un error de acarreo entre campos que un
// puñado de fechas sueltas dejaria pasar.
void test_roundtrips_a_full_day(void) {
  const uint32_t base = kGoodEpoch - (kGoodEpoch % 86400u);
  for (unsigned h = 0; h < 24; h++) {
    roundtrip(base + h * 3600u);
    roundtrip(base + h * 3600u + 3599u);
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_decodes_valid_registers);
  RUN_TEST(test_rejects_voltage_low_flag);
  RUN_TEST(test_rejects_all_ones);
  RUN_TEST(test_rejects_invalid_bcd_nibble);
  RUN_TEST(test_rejects_impossible_month);
  RUN_TEST(test_rejects_nonexistent_date);
  RUN_TEST(test_rejects_feb_29_in_non_leap_year);
  RUN_TEST(test_rejects_year_2000_default);
  RUN_TEST(test_ignores_unused_high_bits);
  RUN_TEST(test_rejects_null_arguments);
  RUN_TEST(test_encodes_valid_epoch);
  RUN_TEST(test_encode_clears_voltage_low);
  RUN_TEST(test_encode_always_clears_century_bit);
  RUN_TEST(test_encode_rejects_out_of_window);
  RUN_TEST(test_roundtrips);
  RUN_TEST(test_roundtrips_a_full_day);
  return UNITY_END();
}

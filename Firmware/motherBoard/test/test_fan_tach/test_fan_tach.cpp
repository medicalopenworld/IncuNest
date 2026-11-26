// Filtro de flancos del tacometro del ventilador (modules/control/fan_tach.h).
//
// Incidencia de campo, unidad 363 (2026-11-26): con el calefactor conmutando,
// el tacometro recibia flancos falsos, fan_rpm saltaba entre 0 y 7041 con el
// ventilador casi parado y el PID bajaba el duty a 0. Estos tests fijan que un
// pulso espurio no produce ni un periodo ni un cambio de nivel, y que los
// flancos reales siguen midiendo el periodo completo.

#include <unity.h>

#include "modules/control/fan_tach.h"

// 4000 rpm con FAN_RPM_CONVERSION = 13333333 us*rpm por medio periodo:
// medio periodo 3333 us, periodo completo (subida a subida) 6667 us.
static const uint32_t HALF_US = 3333;
static const uint32_t FULL_US = 2 * HALF_US;
static const uint32_t MIN_PERIOD_US = 2667; // 10000 rpm

static FanTach t;

void setUp(void) { fan_tach_init(&t); }
void tearDown(void) {}

// Lleva el filtro a un estado con nivel alto conocido y una subida registrada
// en `at`.
static uint32_t prime_rising(uint32_t at) {
  fan_tach_edge(&t, false, at - HALF_US, MIN_PERIOD_US);
  TEST_ASSERT_EQUAL_UINT32(0, fan_tach_edge(&t, true, at, MIN_PERIOD_US));
  return at;
}

static void test_periodo_completo_entre_subidas(void) {
  uint32_t now = prime_rising(100000);
  now += HALF_US;
  TEST_ASSERT_EQUAL_UINT32(0, fan_tach_edge(&t, false, now, MIN_PERIOD_US));
  now += HALF_US;
  TEST_ASSERT_EQUAL_UINT32(FULL_US,
                           fan_tach_edge(&t, true, now, MIN_PERIOD_US));
}

static void test_la_primera_subida_no_da_periodo(void) {
  TEST_ASSERT_EQUAL_UINT32(0, fan_tach_edge(&t, true, 5000, MIN_PERIOD_US));
}

static void test_pulso_espurio_ya_terminado_se_descarta(void) {
  // El pulso dura menos que la espera de la ISR: cuando se lee el pin, ya
  // ha vuelto al nivel de antes. Los dos flancos del pulso llegan con el
  // MISMO nivel que el aceptado y no deben contar.
  uint32_t now = prime_rising(100000);
  TEST_ASSERT_EQUAL_UINT32(0,
                           fan_tach_edge(&t, true, now + 1500, MIN_PERIOD_US));
  TEST_ASSERT_EQUAL_UINT32(0,
                           fan_tach_edge(&t, true, now + 1503, MIN_PERIOD_US));
  TEST_ASSERT_EQUAL_UINT32(2, t.glitches);
  // Y el periodo real siguiente no se ha estropeado.
  TEST_ASSERT_EQUAL_UINT32(
      0, fan_tach_edge(&t, false, now + HALF_US, MIN_PERIOD_US));
  TEST_ASSERT_EQUAL_UINT32(
      FULL_US, fan_tach_edge(&t, true, now + FULL_US, MIN_PERIOD_US));
}

static void test_subida_imposible_de_rapida_no_da_periodo(void) {
  // Un pulso largo (mas que la espera de la ISR) cerca de la subida real:
  // baja y sube de verdad, pero la "subida" llega a 1600 us de la anterior,
  // que serian 16700 rpm. No cuenta, y la referencia sigue siendo la real.
  uint32_t now = prime_rising(100000);
  TEST_ASSERT_EQUAL_UINT32(0,
                           fan_tach_edge(&t, false, now + 1580, MIN_PERIOD_US));
  TEST_ASSERT_EQUAL_UINT32(0,
                           fan_tach_edge(&t, true, now + 1600, MIN_PERIOD_US));
  TEST_ASSERT_EQUAL_UINT32(1, t.glitches);
  TEST_ASSERT_EQUAL_UINT32(
      0, fan_tach_edge(&t, false, now + HALF_US, MIN_PERIOD_US));
  TEST_ASSERT_EQUAL_UINT32(
      FULL_US, fan_tach_edge(&t, true, now + FULL_US, MIN_PERIOD_US));
}

static void test_rafaga_de_ruido_a_400hz_no_corrompe_la_medida(void) {
  // Simulacion: ventilador a 4000 rpm durante 1 s con un pulso espurio de
  // 3 us en cada flanco del PWM del calefactor (400 Hz -> cada 1250 us). La
  // ISR lee el pin tras su espera, cuando el pulso ya ha terminado.
  uint32_t periods = 0, bad = 0;
  uint32_t next_real = 100000, next_noise = 100000 + 700;
  bool level = false;
  while (next_real < 1100000) {
    if (next_noise < next_real) {
      // Los dos flancos del pulso: el pin vuelve a `level` antes de leerse.
      fan_tach_edge(&t, level, next_noise, MIN_PERIOD_US);
      fan_tach_edge(&t, level, next_noise + 3, MIN_PERIOD_US);
      next_noise += 1250;
    } else {
      level = !level;
      const uint32_t p = fan_tach_edge(&t, level, next_real, MIN_PERIOD_US);
      if (p != 0) {
        periods++;
        if (p != FULL_US) bad++;
      }
      next_real += HALF_US;
    }
  }
  TEST_ASSERT_GREATER_THAN_UINT32(140, periods);
  TEST_ASSERT_EQUAL_UINT32(0, bad);
  TEST_ASSERT_GREATER_THAN_UINT32(1000, t.glitches);
}

static void test_desbordamiento_de_micros(void) {
  // micros() desborda a los ~71 min: la resta sin signo tiene que seguir
  // dando el periodo bueno.
  uint32_t now = prime_rising(0xFFFFFFFFu - 1000u);
  fan_tach_edge(&t, false, now + HALF_US, MIN_PERIOD_US);
  TEST_ASSERT_EQUAL_UINT32(
      FULL_US, fan_tach_edge(&t, true, now + FULL_US, MIN_PERIOD_US));
}

static void test_rpm_desde_periodo_completo(void) {
  TEST_ASSERT_EQUAL_UINT32(4000,
                           (uint32_t)(fan_tach_rpm(FULL_US, 13333333.0) + 0.5));
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)fan_tach_rpm(0, 13333333.0));
}

static void test_puntero_nulo(void) {
  TEST_ASSERT_EQUAL_UINT32(0, fan_tach_edge(NULL, true, 1, MIN_PERIOD_US));
  fan_tach_init(NULL);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_periodo_completo_entre_subidas);
  RUN_TEST(test_la_primera_subida_no_da_periodo);
  RUN_TEST(test_pulso_espurio_ya_terminado_se_descarta);
  RUN_TEST(test_subida_imposible_de_rapida_no_da_periodo);
  RUN_TEST(test_rafaga_de_ruido_a_400hz_no_corrompe_la_medida);
  RUN_TEST(test_desbordamiento_de_micros);
  RUN_TEST(test_rpm_desde_periodo_completo);
  RUN_TEST(test_puntero_nulo);
  return UNITY_END();
}

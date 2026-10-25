#include <unity.h>

#include "modules/brightness/brightness_policy.h"

void setUp(void) {}
void tearDown(void) {}

// Entrada base: usuario al maximo, modo noche apagado, desbloqueada, sin
// alarma, sin tocar desde hace 1 minuto (de sobra para atenuar si procediera).
static BrightnessInputs base(void) {
  BrightnessInputs in;
  in.userPct = 100;
  in.nightMode = false;
  in.locked = false;
  in.idleSec = 60;
  in.alarmActive = false;
  return in;
}

// --- Saneado ---------------------------------------------------------------

void test_clamp_keeps_range(void) {
  TEST_ASSERT_EQUAL_UINT8(BRIGHTNESS_MIN_PCT, Brightness_ClampPct(0));
  TEST_ASSERT_EQUAL_UINT8(BRIGHTNESS_MIN_PCT, Brightness_ClampPct(19));
  TEST_ASSERT_EQUAL_UINT8(20, Brightness_ClampPct(20));
  TEST_ASSERT_EQUAL_UINT8(55, Brightness_ClampPct(55));
  TEST_ASSERT_EQUAL_UINT8(100, Brightness_ClampPct(100));
  TEST_ASSERT_EQUAL_UINT8(100, Brightness_ClampPct(101));
  TEST_ASSERT_EQUAL_UINT8(100, Brightness_ClampPct(255));
}

// Un valor corrupto de NVS jamas deja la pantalla negra.
void test_target_sanitizes_user_level(void) {
  BrightnessInputs in = base();
  in.userPct = 0;
  TEST_ASSERT_EQUAL_UINT8(BRIGHTNESS_MIN_PCT, Brightness_Target(&in));
  in.userPct = 250;
  TEST_ASSERT_EQUAL_UINT8(100, Brightness_Target(&in));
}

void test_target_null_is_full(void) {
  TEST_ASSERT_EQUAL_UINT8(100, Brightness_Target(nullptr));
}

// --- Politica --------------------------------------------------------------

void test_user_level_when_idle_unlocked(void) {
  BrightnessInputs in = base();
  in.userPct = 60;
  TEST_ASSERT_EQUAL_UINT8(60, Brightness_Target(&in));
}

void test_night_dims_after_delay_when_locked(void) {
  BrightnessInputs in = base();
  in.nightMode = true;
  in.locked = true;
  in.idleSec = BRIGHTNESS_NIGHT_DELAY_S;
  TEST_ASSERT_EQUAL_UINT8(BRIGHTNESS_NIGHT_PCT, Brightness_Target(&in));
}

void test_night_does_not_dim_before_delay(void) {
  BrightnessInputs in = base();
  in.nightMode = true;
  in.locked = true;
  in.idleSec = BRIGHTNESS_NIGHT_DELAY_S - 1;
  TEST_ASSERT_EQUAL_UINT8(100, Brightness_Target(&in));
}

void test_night_off_never_dims(void) {
  BrightnessInputs in = base();
  in.nightMode = false;
  in.locked = true;
  in.idleSec = 3600;
  TEST_ASSERT_EQUAL_UINT8(100, Brightness_Target(&in));
}

void test_night_does_not_dim_when_not_locked(void) {
  BrightnessInputs in = base();
  in.nightMode = true;
  in.locked = false;
  in.idleSec = 3600;
  TEST_ASSERT_EQUAL_UINT8(100, Brightness_Target(&in));
}

// Nunca mas brillante que lo que eligio el usuario.
void test_night_never_brighter_than_user(void) {
  BrightnessInputs in = base();
  in.nightMode = true;
  in.locked = true;
  in.idleSec = 120;
  in.userPct = 20;
  TEST_ASSERT_EQUAL_UINT8(20, Brightness_Target(&in));
}

// Un toque (idle a 0) en la pantalla de bloqueo atenuada devuelve el nivel.
void test_touch_restores_user_level(void) {
  BrightnessInputs in = base();
  in.nightMode = true;
  in.locked = true;
  in.userPct = 80;
  in.idleSec = 120;
  TEST_ASSERT_EQUAL_UINT8(BRIGHTNESS_NIGHT_PCT, Brightness_Target(&in));
  in.idleSec = 0;
  TEST_ASSERT_EQUAL_UINT8(80, Brightness_Target(&in));
}

// --- Alarma ----------------------------------------------------------------

void test_alarm_forces_full_over_night_dim(void) {
  BrightnessInputs in = base();
  in.nightMode = true;
  in.locked = true;
  in.idleSec = 120;
  in.alarmActive = true;
  TEST_ASSERT_EQUAL_UINT8(100, Brightness_Target(&in));
}

void test_alarm_forces_full_over_low_user_level(void) {
  BrightnessInputs in = base();
  in.userPct = 20;
  in.alarmActive = true;
  TEST_ASSERT_EQUAL_UINT8(100, Brightness_Target(&in));
}

void test_alarm_end_returns_to_night_level(void) {
  BrightnessInputs in = base();
  in.nightMode = true;
  in.locked = true;
  in.idleSec = 120;
  in.alarmActive = true;
  TEST_ASSERT_EQUAL_UINT8(100, Brightness_Target(&in));
  in.alarmActive = false;
  TEST_ASSERT_EQUAL_UINT8(BRIGHTNESS_NIGHT_PCT, Brightness_Target(&in));
}

// Invariante: con alarma, ninguna combinacion de entradas atenua.
void test_alarm_invariant_exhaustive(void) {
  for (int user = 0; user <= 255; user += 5) {
    for (int night = 0; night < 2; night++) {
      for (int locked = 0; locked < 2; locked++) {
        for (uint32_t idle = 0; idle <= 200; idle += 10) {
          BrightnessInputs in;
          in.userPct = (uint8_t)user;
          in.nightMode = night != 0;
          in.locked = locked != 0;
          in.idleSec = idle;
          in.alarmActive = true;
          TEST_ASSERT_EQUAL_UINT8(100, Brightness_Target(&in));
        }
      }
    }
  }
}

// Invariante: sin alarma, el resultado siempre cae en [MIN, MAX].
void test_target_always_in_range(void) {
  for (int user = 0; user <= 255; user += 5) {
    for (int night = 0; night < 2; night++) {
      for (int locked = 0; locked < 2; locked++) {
        BrightnessInputs in;
        in.userPct = (uint8_t)user;
        in.nightMode = night != 0;
        in.locked = locked != 0;
        in.idleSec = 100;
        in.alarmActive = false;
        const uint8_t t = Brightness_Target(&in);
        TEST_ASSERT_TRUE(t >= BRIGHTNESS_MIN_PCT && t <= BRIGHTNESS_MAX_PCT);
      }
    }
  }
}

// --- Conversion a byte -----------------------------------------------------

void test_full_is_the_historic_on_byte(void) {
  TEST_ASSERT_EQUAL_UINT8(BRIGHTNESS_BYTE_FULL,
                          Brightness_PercentToByte(100, true));
  TEST_ASSERT_EQUAL_UINT8(10, Brightness_PercentToByte(100, true));
}

void test_known_points(void) {
  TEST_ASSERT_EQUAL_UINT8(95, Brightness_PercentToByte(80, true));
  TEST_ASSERT_EQUAL_UINT8(160, Brightness_PercentToByte(60, true));
  TEST_ASSERT_EQUAL_UINT8(207, Brightness_PercentToByte(40, true));
  TEST_ASSERT_EQUAL_UINT8(236, Brightness_PercentToByte(20, true));
}

// Ningun porcentaje, ni siquiera basura, apaga la pantalla (245) ni cae en los
// opcodes del zumbador/altavoz (>= 246).
void test_byte_always_bounded_and_monotonic(void) {
  uint8_t prev = 0;
  for (int pct = 0; pct <= 255; pct++) {
    const uint8_t b = Brightness_PercentToByte((uint8_t)pct, true);
    TEST_ASSERT_TRUE(b >= BRIGHTNESS_BYTE_FULL);
    TEST_ASSERT_TRUE(b <= BRIGHTNESS_BYTE_DIMMEST);
    TEST_ASSERT_TRUE(b < BRIGHTNESS_BYTE_OFF);
    if (pct > 0) {
      TEST_ASSERT_TRUE(b <= prev); // mas porcentaje, byte menor o igual
    }
    prev = b;
  }
}

void test_over_100_is_full(void) {
  TEST_ASSERT_EQUAL_UINT8(BRIGHTNESS_BYTE_FULL,
                          Brightness_PercentToByte(200, true));
}

void test_no_level_support_always_full(void) {
  for (int pct = 0; pct <= 255; pct++) {
    TEST_ASSERT_EQUAL_UINT8(BRIGHTNESS_BYTE_FULL,
                            Brightness_PercentToByte((uint8_t)pct, false));
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_clamp_keeps_range);
  RUN_TEST(test_target_sanitizes_user_level);
  RUN_TEST(test_target_null_is_full);
  RUN_TEST(test_user_level_when_idle_unlocked);
  RUN_TEST(test_night_dims_after_delay_when_locked);
  RUN_TEST(test_night_does_not_dim_before_delay);
  RUN_TEST(test_night_off_never_dims);
  RUN_TEST(test_night_does_not_dim_when_not_locked);
  RUN_TEST(test_night_never_brighter_than_user);
  RUN_TEST(test_touch_restores_user_level);
  RUN_TEST(test_alarm_forces_full_over_night_dim);
  RUN_TEST(test_alarm_forces_full_over_low_user_level);
  RUN_TEST(test_alarm_end_returns_to_night_level);
  RUN_TEST(test_alarm_invariant_exhaustive);
  RUN_TEST(test_target_always_in_range);
  RUN_TEST(test_full_is_the_historic_on_byte);
  RUN_TEST(test_known_points);
  RUN_TEST(test_byte_always_bounded_and_monotonic);
  RUN_TEST(test_over_100_is_full);
  RUN_TEST(test_no_level_support_always_full);
  return UNITY_END();
}

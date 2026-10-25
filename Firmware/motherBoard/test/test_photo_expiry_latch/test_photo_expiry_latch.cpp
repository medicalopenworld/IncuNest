// Regresion: la caducidad del temporizador de fototerapia no puede perder
// frente a una trama rezagada del display con photo=1 y sin minutos.
// Ver modules/control/photo_expiry_latch.h.

#include <unity.h>

#include "modules/control/photo_expiry_latch.h"

static PhotoExpiryLatch l;

void setUp(void) { photo_expiry_latch_init(&l); }
void tearDown(void) {}

// Sin caducidad el filtro no toca nada.
void test_without_expiry_everything_passes(void) {
  TEST_ASSERT_TRUE(photo_expiry_latch_filter(&l, true, 0));
  TEST_ASSERT_TRUE(photo_expiry_latch_filter(&l, true, 15));
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, false, 0));
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, false, 15));
}

// Caduca, llega una trama rezagada photo=1 photoMin=0 y la lampara sigue OFF.
void test_expiry_then_stale_on_without_minutes_stays_off(void) {
  photo_expiry_latch_expire(&l);
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, true, 0));
}

// Y sigue ignorandose mientras el display no haya dicho 0 (la trama retenida
// 10 s por localCmdHoldsLocal llega varias veces).
void test_stale_on_is_ignored_repeatedly_until_a_zero(void) {
  photo_expiry_latch_expire(&l);
  for (unsigned i = 0; i < 20u; ++i)
    TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, true, 0));
}

// Llega photo=0 (el display ya adopto el apagado) y despues photo=1 con
// minutos: se enciende.
void test_zero_then_timed_on_turns_on(void) {
  photo_expiry_latch_expire(&l);
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, true, 0));
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, false, 0));
  TEST_ASSERT_TRUE(photo_expiry_latch_filter(&l, true, 15));
}

// Tras ver el 0, un photo=1 sin minutos ya es una sesion continua legitima.
void test_zero_then_continuous_on_turns_on(void) {
  photo_expiry_latch_expire(&l);
  photo_expiry_latch_filter(&l, false, 0);
  TEST_ASSERT_TRUE(photo_expiry_latch_filter(&l, true, 0));
}

// Una sesion nueva con temporizador es una orden inequivoca: se acepta aun sin
// haber visto el 0, y cierra el cerrojo.
void test_timed_on_without_a_zero_is_accepted_and_clears_the_latch(void) {
  photo_expiry_latch_expire(&l);
  TEST_ASSERT_TRUE(photo_expiry_latch_filter(&l, true, 20));
  TEST_ASSERT_TRUE(photo_expiry_latch_filter(&l, true, 0));
}

// photo=0 siempre pasa y nunca reabre nada.
void test_off_always_passes(void) {
  photo_expiry_latch_expire(&l);
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, false, 0));
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, false, 15));
}

// Caducar dos veces seguidas es lo mismo que una.
void test_expire_is_idempotent(void) {
  photo_expiry_latch_expire(&l);
  photo_expiry_latch_expire(&l);
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, true, 0));
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, false, 0));
  TEST_ASSERT_TRUE(photo_expiry_latch_filter(&l, true, 0));
}

// Una caducidad posterior vuelve a armar el cerrojo.
void test_second_expiry_rearms(void) {
  photo_expiry_latch_expire(&l);
  photo_expiry_latch_filter(&l, false, 0);
  TEST_ASSERT_TRUE(photo_expiry_latch_filter(&l, true, 10));
  photo_expiry_latch_expire(&l);
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, true, 0));
}

// --- M1: persistir OFF al caducar, UNA vez ---

// Al caducar hay que guardar KEY_PHOTO_ACTIVE=0 (si la placa cae antes de la
// siguiente trama del display, arrancaria en fototerapia continua sin limite).
void test_expiry_arms_a_pending_persist_off_taken_once(void) {
  TEST_ASSERT_FALSE(photo_expiry_latch_take_persist_off(&l));
  photo_expiry_latch_expire(&l);
  TEST_ASSERT_TRUE(photo_expiry_latch_take_persist_off(&l));
  TEST_ASSERT_FALSE(photo_expiry_latch_take_persist_off(&l));
}

// Dos tareas detectan la caducidad a la vez: una sola escritura.
void test_double_expire_in_the_same_episode_persists_once(void) {
  photo_expiry_latch_expire(&l);
  photo_expiry_latch_expire(&l);
  TEST_ASSERT_TRUE(photo_expiry_latch_take_persist_off(&l));
  photo_expiry_latch_expire(&l);  // aun dentro del episodio
  TEST_ASSERT_FALSE(photo_expiry_latch_take_persist_off(&l));
}

// Otro episodio (tras cerrar el cerrojo) vuelve a pedirlo.
void test_next_episode_persists_again(void) {
  photo_expiry_latch_expire(&l);
  photo_expiry_latch_take_persist_off(&l);
  photo_expiry_latch_filter(&l, false, 0);  // el display adopta el apagado
  photo_expiry_latch_expire(&l);
  TEST_ASSERT_TRUE(photo_expiry_latch_take_persist_off(&l));
}

// --- B4: un log por episodio ---

void test_filtering_logs_once_per_episode(void) {
  TEST_ASSERT_FALSE(photo_expiry_latch_take_filter_log(&l));
  photo_expiry_latch_expire(&l);
  // Sin trama filtrada todavia, nada que loguear.
  TEST_ASSERT_FALSE(photo_expiry_latch_take_filter_log(&l));
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(&l, true, 0));
  TEST_ASSERT_TRUE(photo_expiry_latch_take_filter_log(&l));
  // La trama retenida llega muchas veces: ni un log mas en este episodio.
  for (unsigned i = 0; i < 20u; ++i) {
    photo_expiry_latch_filter(&l, true, 0);
    TEST_ASSERT_FALSE(photo_expiry_latch_take_filter_log(&l));
  }
}

void test_next_episode_logs_again(void) {
  photo_expiry_latch_expire(&l);
  photo_expiry_latch_filter(&l, true, 0);
  photo_expiry_latch_take_filter_log(&l);
  photo_expiry_latch_filter(&l, false, 0);
  photo_expiry_latch_expire(&l);
  photo_expiry_latch_filter(&l, true, 0);
  TEST_ASSERT_TRUE(photo_expiry_latch_take_filter_log(&l));
}

// Una trama que pasa (con minutos) no genera log.
void test_accepted_frames_never_log(void) {
  photo_expiry_latch_expire(&l);
  photo_expiry_latch_filter(&l, true, 15);
  photo_expiry_latch_filter(&l, false, 0);
  TEST_ASSERT_FALSE(photo_expiry_latch_take_filter_log(&l));
}

// --- B1: validar photo y photoMin, y descartar la trama entera ---

void test_valid_fields_are_accepted(void) {
  TEST_ASSERT_TRUE(photo_hmi_fields_valid(0, 0));
  TEST_ASSERT_TRUE(photo_hmi_fields_valid(1, 0));
  TEST_ASSERT_TRUE(photo_hmi_fields_valid(1, 15));
  TEST_ASSERT_TRUE(photo_hmi_fields_valid(1, PHOTO_TIMER_MAX_MINUTES));
  TEST_ASSERT_TRUE(photo_hmi_fields_valid(0, PHOTO_TIMER_MAX_MINUTES));
}

void test_photo_out_of_range_is_rejected(void) {
  TEST_ASSERT_FALSE(photo_hmi_fields_valid(2, 0));
  TEST_ASSERT_FALSE(photo_hmi_fields_valid(-1, 0));
  TEST_ASSERT_FALSE(photo_hmi_fields_valid(255, 10));
}

void test_photo_min_out_of_range_is_rejected(void) {
  TEST_ASSERT_FALSE(photo_hmi_fields_valid(1, -1));
  TEST_ASSERT_FALSE(photo_hmi_fields_valid(1, PHOTO_TIMER_MAX_MINUTES + 1));
  TEST_ASSERT_FALSE(photo_hmi_fields_valid(0, 99999));
  TEST_ASSERT_FALSE(photo_hmi_fields_valid(1, -2147483647 - 1));
}

void test_null_is_harmless(void) {
  TEST_ASSERT_FALSE(photo_expiry_latch_take_persist_off(NULL));
  TEST_ASSERT_FALSE(photo_expiry_latch_take_filter_log(NULL));
  photo_expiry_latch_init(NULL);
  photo_expiry_latch_expire(NULL);
  TEST_ASSERT_TRUE(photo_expiry_latch_filter(NULL, true, 0));
  TEST_ASSERT_FALSE(photo_expiry_latch_filter(NULL, false, 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_without_expiry_everything_passes);
  RUN_TEST(test_expiry_then_stale_on_without_minutes_stays_off);
  RUN_TEST(test_stale_on_is_ignored_repeatedly_until_a_zero);
  RUN_TEST(test_zero_then_timed_on_turns_on);
  RUN_TEST(test_zero_then_continuous_on_turns_on);
  RUN_TEST(test_timed_on_without_a_zero_is_accepted_and_clears_the_latch);
  RUN_TEST(test_off_always_passes);
  RUN_TEST(test_expire_is_idempotent);
  RUN_TEST(test_second_expiry_rearms);
  RUN_TEST(test_expiry_arms_a_pending_persist_off_taken_once);
  RUN_TEST(test_double_expire_in_the_same_episode_persists_once);
  RUN_TEST(test_next_episode_persists_again);
  RUN_TEST(test_filtering_logs_once_per_episode);
  RUN_TEST(test_next_episode_logs_again);
  RUN_TEST(test_accepted_frames_never_log);
  RUN_TEST(test_valid_fields_are_accepted);
  RUN_TEST(test_photo_out_of_range_is_rejected);
  RUN_TEST(test_photo_min_out_of_range_is_rejected);
  RUN_TEST(test_null_is_harmless);
  return UNITY_END();
}

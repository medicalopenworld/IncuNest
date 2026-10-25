#include <unity.h>

#include "modules/maintenance/weigh_state.h"

void setUp(void) {}
void tearDown(void) {}

static const uint32_t NOW = 1789482645u;  // 2026-09-16T14:30:45Z
static const uint32_t H = 3600u;
static const uint32_t DAY = 86400u;
static const uint32_t A = 7u;
static const uint32_t B = 9u;

static weigh_state_t fresh(void) {
  weigh_state_t s;
  weigh_state_init(&s);
  return s;
}

static weigh_env_t env(uint32_t now, uint32_t active, bool training = false) {
  weigh_env_t e = {};
  e.nowEpoch = now;
  e.activeSeq = active;
  e.training = training;
  return e;
}

// --- Estado de fabrica -----------------------------------------------------

void test_factory_state_is_enabled_and_empty(void) {
  weigh_state_t s = fresh();
  TEST_ASSERT_TRUE(s.enabled);
  TEST_ASSERT_EQUAL_UINT32(0, s.weighEpoch);
  TEST_ASSERT_EQUAL_UINT32(0, s.seenEpoch);
  TEST_ASSERT_EQUAL_UINT32(0, s.snoozeUntil);
  TEST_ASSERT_FALSE(s.dirty);
}

// --- Visto por primera vez -------------------------------------------------

void test_first_seen_is_stamped_once(void) {
  weigh_state_t s = fresh();
  weigh_env_t e = env(NOW, A);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(A, s.seenSeq);
  TEST_ASSERT_EQUAL_UINT32(NOW, s.seenEpoch);
  TEST_ASSERT_TRUE(s.dirty);

  s.dirty = false;
  e = env(NOW + H, A);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(NOW, s.seenEpoch);  // no se pisa
  TEST_ASSERT_FALSE(s.dirty);
}

void test_first_seen_waits_for_the_clock(void) {
  weigh_state_t s = fresh();
  weigh_env_t e = env(0, A);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(0, s.seenSeq);
  e = env(NOW, A);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(NOW, s.seenEpoch);
}

// --- Alta y cambio de bebe -------------------------------------------------

void test_discharge_clears_everything(void) {
  weigh_state_t s = fresh();
  weigh_env_t e = env(NOW, A);
  weigh_state_tick(&s, &e);
  weigh_state_note_weighed(&s, &e, A, 1800, false);
  weigh_state_snooze(&s, NOW);
  s.dirty = false;

  e = env(NOW + H, 0);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(0, s.weighSeq);
  TEST_ASSERT_EQUAL_UINT32(0, s.weighEpoch);
  TEST_ASSERT_EQUAL_UINT32(0, s.seenSeq);
  TEST_ASSERT_EQUAL_UINT32(0, s.seenEpoch);
  TEST_ASSERT_EQUAL_UINT32(0, s.snoozeUntil);
  TEST_ASSERT_TRUE(s.dirty);
}

void test_new_baby_drops_what_is_not_his(void) {
  weigh_state_t s = fresh();
  weigh_env_t e = env(NOW, A);
  weigh_state_tick(&s, &e);
  weigh_state_note_weighed(&s, &e, A, 1800, false);
  weigh_state_snooze(&s, NOW);

  e = env(NOW + H, B);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(0, s.weighSeq);
  TEST_ASSERT_EQUAL_UINT32(0, s.weighEpoch);
  TEST_ASSERT_EQUAL_UINT32(B, s.seenSeq);
  TEST_ASSERT_EQUAL_UINT32(NOW + H, s.seenEpoch);
  TEST_ASSERT_EQUAL_UINT32(0, s.snoozeUntil);
}

// Reinicio del HMI: el estado viene de NVS con el bebe A, y el primer seq
// observado es el mismo A. Ni sello ni "mas tarde" se pierden.
void test_same_baby_after_restart_keeps_stamp_and_snooze(void) {
  weigh_state_t s = fresh();
  s.weighSeq = A;
  s.weighEpoch = NOW - 2 * H;
  s.seenSeq = A;
  s.seenEpoch = NOW - 5 * DAY;
  s.snoozeUntil = NOW + H;
  weigh_env_t e = env(NOW, A);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(NOW - 2 * H, s.weighEpoch);
  TEST_ASSERT_EQUAL_UINT32(NOW + H, s.snoozeUntil);
  TEST_ASSERT_EQUAL_UINT32(NOW - 5 * DAY, s.seenEpoch);
}

// Otro bebe tras un reinicio: se descarta lo ajeno.
void test_other_baby_after_restart_drops_old_stamp(void) {
  weigh_state_t s = fresh();
  s.weighSeq = A;
  s.weighEpoch = NOW - 2 * H;
  s.snoozeUntil = NOW + H;
  weigh_env_t e = env(NOW, B);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(0, s.weighEpoch);
  TEST_ASSERT_EQUAL_UINT32(0, s.snoozeUntil);
}

// --- Sellar una pesada -----------------------------------------------------

void test_weighing_stamps_epoch_and_clears_snooze(void) {
  weigh_state_t s = fresh();
  weigh_env_t e = env(NOW, A);
  weigh_state_tick(&s, &e);
  weigh_state_snooze(&s, NOW);
  s.dirty = false;

  e = env(NOW + 10, A);
  TEST_ASSERT_TRUE(weigh_state_note_weighed(&s, &e, A, 1900, false));
  TEST_ASSERT_EQUAL_UINT32(A, s.weighSeq);
  TEST_ASSERT_EQUAL_UINT32(NOW + 10, s.weighEpoch);
  TEST_ASSERT_EQUAL_UINT32(0, s.snoozeUntil);
  TEST_ASSERT_TRUE(s.dirty);
}

void test_skip_does_not_stamp(void) {
  weigh_state_t s = fresh();
  weigh_env_t e = env(NOW, A);
  TEST_ASSERT_FALSE(weigh_state_note_weighed(&s, &e, A, 0, false));
  TEST_ASSERT_EQUAL_UINT32(0, s.weighEpoch);
  TEST_ASSERT_FALSE(s.dirty);
}

void test_weight_of_a_baby_not_in_charge_does_not_stamp(void) {
  weigh_state_t s = fresh();
  weigh_env_t e = env(NOW, A);
  weigh_state_tick(&s, &e);
  weigh_state_note_weighed(&s, &e, A, 1800, false);
  s.dirty = false;
  TEST_ASSERT_FALSE(weigh_state_note_weighed(&s, &e, B, 2000, false));
  TEST_ASSERT_EQUAL_UINT32(A, s.weighSeq);
  TEST_ASSERT_FALSE(s.dirty);
}

void test_no_baby_in_charge_does_not_stamp(void) {
  weigh_state_t s = fresh();
  weigh_env_t e = env(NOW, 0);
  TEST_ASSERT_FALSE(weigh_state_note_weighed(&s, &e, A, 1800, false));
  TEST_ASSERT_EQUAL_UINT32(0, s.weighSeq);
}

void test_training_never_stamps_or_ticks(void) {
  weigh_state_t s = fresh();
  weigh_env_t e = env(NOW, A, true);
  TEST_ASSERT_FALSE(weigh_state_note_weighed(&s, &e, A, 1800, false));
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(0, s.weighSeq);
  TEST_ASSERT_EQUAL_UINT32(0, s.seenSeq);
  TEST_ASSERT_EQUAL_UINT32(0, s.trackedSeq);
  TEST_ASSERT_FALSE(s.dirty);
}

void test_practice_seq_never_stamps(void) {
  weigh_state_t s = fresh();
  weigh_env_t e = env(NOW, A);
  TEST_ASSERT_FALSE(weigh_state_note_weighed(&s, &e, A, 1800, true));
  TEST_ASSERT_EQUAL_UINT32(0, s.weighSeq);
}

// El asistente admite al bebe y pesa antes de que tick() lo haya visto al
// mando: el sello no se pierde cuando tick() llega.
void test_stamp_before_first_tick_survives_it(void) {
  weigh_state_t s = fresh();
  weigh_env_t e = env(NOW, B);
  TEST_ASSERT_TRUE(weigh_state_note_weighed(&s, &e, B, 1500, false));
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(B, s.weighSeq);
  TEST_ASSERT_EQUAL_UINT32(NOW, s.weighEpoch);
}

// --- Pesada sin hora -------------------------------------------------------

void test_weighing_without_clock_is_pending_and_silences_warning(void) {
  weigh_state_t s = fresh();
  s.weighSeq = A;
  s.weighEpoch = NOW - 3 * DAY;
  s.trackedSeq = A;
  weigh_env_t noClock = env(0, A);
  TEST_ASSERT_TRUE(weigh_state_note_weighed(&s, &noClock, A, 1800, false));
  TEST_ASSERT_EQUAL_UINT32(A, s.pendingSeq);
  TEST_ASSERT_EQUAL_UINT32(NOW - 3 * DAY, s.weighEpoch);  // aun sin fechar

  // Con hora, y con el sello viejo vencido, no avisa mientras haya pendiente.
  weigh_env_t e = env(NOW, A);
  weigh_state_t copy = s;
  TEST_ASSERT_FALSE(weigh_state_should_warn(&copy, &e, true));
}

void test_pending_weighing_is_stamped_with_first_valid_time(void) {
  weigh_state_t s = fresh();
  s.trackedSeq = A;
  s.seenSeq = A;
  s.seenEpoch = NOW - 3 * DAY;
  weigh_env_t noClock = env(0, A);
  weigh_state_note_weighed(&s, &noClock, A, 1800, false);

  weigh_env_t e = env(NOW, A);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(0, s.pendingSeq);
  TEST_ASSERT_EQUAL_UINT32(A, s.weighSeq);
  TEST_ASSERT_EQUAL_UINT32(NOW, s.weighEpoch);
  TEST_ASSERT_FALSE(weigh_state_should_warn(&s, &e, true));
}

void test_pending_weighing_is_dropped_if_the_baby_changes(void) {
  weigh_state_t s = fresh();
  s.trackedSeq = A;
  weigh_env_t noClock = env(0, A);
  weigh_state_note_weighed(&s, &noClock, A, 1800, false);
  weigh_env_t e = env(NOW, B);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(0, s.pendingSeq);
  TEST_ASSERT_EQUAL_UINT32(0, s.weighEpoch);
}

// --- Reloj en el futuro ----------------------------------------------------

void test_future_stamp_beyond_tolerance_is_reanchored(void) {
  weigh_state_t s = fresh();
  s.trackedSeq = A;
  s.seenSeq = A;
  s.seenEpoch = NOW;
  s.weighSeq = A;
  s.weighEpoch = NOW + 61;
  weigh_env_t e = env(NOW, A);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(NOW, s.weighEpoch);
  TEST_ASSERT_TRUE(s.dirty);
}

void test_future_stamp_within_tolerance_is_left_alone(void) {
  weigh_state_t s = fresh();
  s.trackedSeq = A;
  s.seenSeq = A;
  s.seenEpoch = NOW;
  s.weighSeq = A;
  s.weighEpoch = NOW + 60;
  weigh_env_t e = env(NOW, A);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(NOW + 60, s.weighEpoch);
  TEST_ASSERT_FALSE(s.dirty);
}

void test_future_seen_and_snooze_are_reanchored(void) {
  weigh_state_t s = fresh();
  s.trackedSeq = A;
  s.seenSeq = A;
  s.seenEpoch = NOW + 5 * DAY;
  s.snoozeUntil = NOW + 5 * DAY;
  weigh_env_t e = env(NOW, A);
  weigh_state_tick(&s, &e);
  TEST_ASSERT_EQUAL_UINT32(NOW, s.seenEpoch);
  TEST_ASSERT_EQUAL_UINT32(0, s.snoozeUntil);
}

// --- Mas tarde e interruptor ----------------------------------------------

void test_snooze_needs_a_clock(void) {
  weigh_state_t s = fresh();
  TEST_ASSERT_FALSE(weigh_state_snooze(&s, 0));
  TEST_ASSERT_EQUAL_UINT32(0, s.snoozeUntil);
  TEST_ASSERT_FALSE(s.dirty);
  TEST_ASSERT_TRUE(weigh_state_snooze(&s, NOW));
  TEST_ASSERT_EQUAL_UINT32(NOW + DAY, s.snoozeUntil);
  TEST_ASSERT_TRUE(s.dirty);
}

void test_disabled_never_warns_and_enabling_clears_snooze(void) {
  weigh_state_t s = fresh();
  s.trackedSeq = A;
  s.weighSeq = A;
  s.weighEpoch = NOW - 3 * DAY;
  weigh_env_t e = env(NOW, A);
  TEST_ASSERT_TRUE(weigh_state_should_warn(&s, &e, true));

  weigh_state_set_enabled(&s, false);
  TEST_ASSERT_FALSE(s.enabled);
  TEST_ASSERT_FALSE(weigh_state_should_warn(&s, &e, true));

  s.snoozeUntil = NOW + 10 * H;
  weigh_state_set_enabled(&s, true);
  TEST_ASSERT_TRUE(s.enabled);
  TEST_ASSERT_EQUAL_UINT32(0, s.snoozeUntil);
  TEST_ASSERT_TRUE(weigh_state_should_warn(&s, &e, true));
}

void test_should_warn_needs_live_session(void) {
  weigh_state_t s = fresh();
  s.trackedSeq = A;
  s.weighSeq = A;
  s.weighEpoch = NOW - 3 * DAY;
  weigh_env_t e = env(NOW, A);
  TEST_ASSERT_FALSE(weigh_state_should_warn(&s, &e, false));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_factory_state_is_enabled_and_empty);
  RUN_TEST(test_first_seen_is_stamped_once);
  RUN_TEST(test_first_seen_waits_for_the_clock);
  RUN_TEST(test_discharge_clears_everything);
  RUN_TEST(test_new_baby_drops_what_is_not_his);
  RUN_TEST(test_same_baby_after_restart_keeps_stamp_and_snooze);
  RUN_TEST(test_other_baby_after_restart_drops_old_stamp);
  RUN_TEST(test_weighing_stamps_epoch_and_clears_snooze);
  RUN_TEST(test_skip_does_not_stamp);
  RUN_TEST(test_weight_of_a_baby_not_in_charge_does_not_stamp);
  RUN_TEST(test_no_baby_in_charge_does_not_stamp);
  RUN_TEST(test_training_never_stamps_or_ticks);
  RUN_TEST(test_practice_seq_never_stamps);
  RUN_TEST(test_stamp_before_first_tick_survives_it);
  RUN_TEST(test_weighing_without_clock_is_pending_and_silences_warning);
  RUN_TEST(test_pending_weighing_is_stamped_with_first_valid_time);
  RUN_TEST(test_pending_weighing_is_dropped_if_the_baby_changes);
  RUN_TEST(test_future_stamp_beyond_tolerance_is_reanchored);
  RUN_TEST(test_future_stamp_within_tolerance_is_left_alone);
  RUN_TEST(test_future_seen_and_snooze_are_reanchored);
  RUN_TEST(test_snooze_needs_a_clock);
  RUN_TEST(test_disabled_never_warns_and_enabling_clears_snooze);
  RUN_TEST(test_should_warn_needs_live_session);
  return UNITY_END();
}

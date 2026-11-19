#include <unity.h>

#include "modules/maintenance/weigh_reminder.h"

void setUp(void) {}
void tearDown(void) {}

static const uint32_t NOW = 1789482645u;  // 2026-09-16T14:30:45Z
static const uint32_t H = 3600u;
static const uint32_t DAY = 86400u;
static const uint32_t SEQ = 7u;

// Bebe activo con sesion viva y una pesada sellada hace `agoSecs`.
static weigh_inputs_t weighedAgo(uint32_t agoSecs) {
  weigh_inputs_t in = {};
  in.nowEpoch = NOW;
  in.activeSeq = SEQ;
  in.liveSession = true;
  in.weighSeq = SEQ;
  in.weighEpoch = NOW - agoSecs;
  return in;
}

// Bebe activo sin pesada sellada, visto por primera vez hace `agoSecs`.
static weigh_inputs_t seenAgo(uint32_t agoSecs) {
  weigh_inputs_t in = {};
  in.nowEpoch = NOW;
  in.activeSeq = SEQ;
  in.liveSession = true;
  in.seenSeq = SEQ;
  in.seenEpoch = NOW - agoSecs;
  return in;
}

// --- Plazo de 24 h ---------------------------------------------------------

void test_does_not_warn_one_second_before_24h(void) {
  weigh_inputs_t in = weighedAgo(DAY - 1);
  TEST_ASSERT_FALSE(weigh_reminder_due(&in));
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

void test_warns_at_exactly_24h(void) {
  weigh_inputs_t in = weighedAgo(DAY);
  TEST_ASSERT_TRUE(weigh_reminder_due(&in));
  TEST_ASSERT_TRUE(weigh_reminder_should_warn(&in));
}

void test_warns_after_several_days(void) {
  weigh_inputs_t in = weighedAgo(3 * DAY);
  TEST_ASSERT_TRUE(weigh_reminder_should_warn(&in));
}

void test_does_not_warn_just_after_weighing(void) {
  weigh_inputs_t in = weighedAgo(0);
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

// --- Sin hora, sin sesion --------------------------------------------------

void test_never_warns_without_clock(void) {
  weigh_inputs_t in = weighedAgo(3 * DAY);
  in.nowEpoch = 0;
  TEST_ASSERT_FALSE(weigh_reminder_due(&in));
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

void test_does_not_warn_without_live_session(void) {
  weigh_inputs_t in = weighedAgo(3 * DAY);
  in.liveSession = false;
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

void test_does_not_warn_without_active_baby(void) {
  weigh_inputs_t in = weighedAgo(3 * DAY);
  in.activeSeq = 0;
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

// --- Referencia ------------------------------------------------------------

void test_no_stamp_uses_first_seen(void) {
  weigh_inputs_t old = seenAgo(25 * H);
  TEST_ASSERT_TRUE(weigh_reminder_should_warn(&old));
  weigh_inputs_t fresh = seenAgo(2 * H);
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&fresh));
}

void test_no_reference_at_all_does_not_warn(void) {
  weigh_inputs_t in = {};
  in.nowEpoch = NOW;
  in.activeSeq = SEQ;
  in.liveSession = true;
  TEST_ASSERT_EQUAL_UINT32(0, weigh_reference_epoch(&in));
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

// Un sello de otro bebe no cuenta ni como pesada ni como "visto".
void test_stamp_of_another_baby_is_ignored(void) {
  weigh_inputs_t in = seenAgo(2 * H);
  in.weighSeq = SEQ + 1;
  in.weighEpoch = NOW - 3 * DAY;
  TEST_ASSERT_EQUAL_UINT32(NOW - 2 * H, weigh_reference_epoch(&in));
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

void test_stamp_of_another_baby_alone_gives_no_reference(void) {
  weigh_inputs_t in = weighedAgo(3 * DAY);
  in.weighSeq = SEQ + 1;
  TEST_ASSERT_EQUAL_UINT32(0, weigh_reference_epoch(&in));
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

void test_seen_of_another_baby_is_ignored(void) {
  weigh_inputs_t in = seenAgo(3 * DAY);
  in.seenSeq = SEQ + 1;
  TEST_ASSERT_EQUAL_UINT32(0, weigh_reference_epoch(&in));
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

// La pesada manda sobre el "visto": un bebe visto hace 5 dias pero pesado hace
// 1 h no esta vencido.
void test_stamp_beats_first_seen(void) {
  weigh_inputs_t in = seenAgo(5 * DAY);
  in.weighSeq = SEQ;
  in.weighEpoch = NOW - H;
  TEST_ASSERT_EQUAL_UINT32(NOW - H, weigh_reference_epoch(&in));
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

// Un epoch 0 en el sello es "ninguna pesada", aunque el seq coincida.
void test_zero_stamp_epoch_falls_back_to_seen(void) {
  weigh_inputs_t in = seenAgo(26 * H);
  in.weighSeq = SEQ;
  in.weighEpoch = 0;
  TEST_ASSERT_EQUAL_UINT32(NOW - 26 * H, weigh_reference_epoch(&in));
  TEST_ASSERT_TRUE(weigh_reminder_should_warn(&in));
}

// Reloj corregido hacia atras: la referencia queda en el futuro.
void test_reference_in_the_future_does_not_warn(void) {
  weigh_inputs_t in = weighedAgo(0);
  in.weighEpoch = NOW + H;
  TEST_ASSERT_FALSE(weigh_reminder_due(&in));
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
  TEST_ASSERT_EQUAL_UINT32(0, weigh_elapsed_hours(&in));
}

// --- Mas tarde -------------------------------------------------------------

void test_active_snooze_silences_a_due_reminder(void) {
  weigh_inputs_t in = weighedAgo(2 * DAY);
  in.snoozeUntil = NOW + H;
  TEST_ASSERT_TRUE(weigh_reminder_due(&in));
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

void test_warns_again_exactly_when_snooze_expires(void) {
  weigh_inputs_t in = weighedAgo(2 * DAY);
  in.snoozeUntil = NOW;
  TEST_ASSERT_TRUE(weigh_reminder_should_warn(&in));
  in.snoozeUntil = NOW + 1;
  TEST_ASSERT_FALSE(weigh_reminder_should_warn(&in));
}

void test_snooze_lasts_24h_and_needs_a_clock(void) {
  TEST_ASSERT_EQUAL_UINT32(NOW + DAY, weigh_snooze_until(NOW));
  TEST_ASSERT_EQUAL_UINT32(0, weigh_snooze_until(0));
}

// --- Horas para el texto ---------------------------------------------------

void test_elapsed_hours_are_whole_hours(void) {
  weigh_inputs_t in = weighedAgo(26 * H + 59 * 60);
  TEST_ASSERT_EQUAL_UINT32(26, weigh_elapsed_hours(&in));
}

void test_elapsed_hours_zero_without_clock_or_reference(void) {
  weigh_inputs_t in = weighedAgo(3 * DAY);
  in.nowEpoch = 0;
  TEST_ASSERT_EQUAL_UINT32(0, weigh_elapsed_hours(&in));
  weigh_inputs_t empty = {};
  empty.nowEpoch = NOW;
  empty.activeSeq = SEQ;
  TEST_ASSERT_EQUAL_UINT32(0, weigh_elapsed_hours(&empty));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_does_not_warn_one_second_before_24h);
  RUN_TEST(test_warns_at_exactly_24h);
  RUN_TEST(test_warns_after_several_days);
  RUN_TEST(test_does_not_warn_just_after_weighing);
  RUN_TEST(test_never_warns_without_clock);
  RUN_TEST(test_does_not_warn_without_live_session);
  RUN_TEST(test_does_not_warn_without_active_baby);
  RUN_TEST(test_no_stamp_uses_first_seen);
  RUN_TEST(test_no_reference_at_all_does_not_warn);
  RUN_TEST(test_stamp_of_another_baby_is_ignored);
  RUN_TEST(test_stamp_of_another_baby_alone_gives_no_reference);
  RUN_TEST(test_seen_of_another_baby_is_ignored);
  RUN_TEST(test_stamp_beats_first_seen);
  RUN_TEST(test_zero_stamp_epoch_falls_back_to_seen);
  RUN_TEST(test_reference_in_the_future_does_not_warn);
  RUN_TEST(test_active_snooze_silences_a_due_reminder);
  RUN_TEST(test_warns_again_exactly_when_snooze_expires);
  RUN_TEST(test_snooze_lasts_24h_and_needs_a_clock);
  RUN_TEST(test_elapsed_hours_are_whole_hours);
  RUN_TEST(test_elapsed_hours_zero_without_clock_or_reference);
  return UNITY_END();
}

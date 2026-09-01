#include <unity.h>

#include "modules/sensorboard_comm/sb_link_state.h"
#include "modules/sensorboard_comm/sb_protocol.h"

void setUp(void) {}
void tearDown(void) {}

// Falla-seguro: mientras no llegue el primer heartbeat el SensorBoard no
// cuenta como disponible, aunque el USB haya enumerado.
void test_starts_disconnected(void) {
  SbLinkState s;
  sb_link_state_init(&s);
  TEST_ASSERT_FALSE(sb_link_state_is_connected(&s, 0));
  TEST_ASSERT_FALSE(sb_link_state_is_connected(&s, 1000000));
}

void test_heartbeat_marks_connected(void) {
  SbLinkState s;
  sb_link_state_init(&s);
  sb_link_state_note_heartbeat(&s, 1000);
  TEST_ASSERT_TRUE(sb_link_state_is_connected(&s, 1000));
  TEST_ASSERT_TRUE(sb_link_state_is_connected(&s, 1000 + 30000));
}

// 3 periodos de 30 s: a los 90 s justos todavia vale, pasado eso no.
void test_link_lost_after_three_missed_periods(void) {
  SbLinkState s;
  sb_link_state_init(&s);
  sb_link_state_note_heartbeat(&s, 10000);

  TEST_ASSERT_TRUE(
      sb_link_state_is_connected(&s, 10000 + SB_LINK_TIMEOUT_MS));
  TEST_ASSERT_FALSE(
      sb_link_state_is_connected(&s, 10000 + SB_LINK_TIMEOUT_MS + 1));
}

void test_recovers_on_next_heartbeat(void) {
  SbLinkState s;
  sb_link_state_init(&s);
  sb_link_state_note_heartbeat(&s, 10000);
  TEST_ASSERT_FALSE(sb_link_state_is_connected(&s, 200000));

  sb_link_state_note_heartbeat(&s, 200000);
  TEST_ASSERT_TRUE(sb_link_state_is_connected(&s, 200000));
}

// millis() da la vuelta a los ~49.7 dias; con resta sin signo un hueco de
// 90 s a caballo del vuelco se sigue midiendo bien.
void test_survives_millis_rollover(void) {
  SbLinkState s;
  sb_link_state_init(&s);
  const uint32_t before_wrap = 0xFFFFFF00u;
  sb_link_state_note_heartbeat(&s, before_wrap);

  const uint32_t after_wrap = before_wrap + 30000u;  // ya ha dado la vuelta
  TEST_ASSERT_TRUE(sb_link_state_is_connected(&s, after_wrap));
  TEST_ASSERT_FALSE(
      sb_link_state_is_connected(&s, before_wrap + SB_LINK_TIMEOUT_MS + 1));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_disconnected);
  RUN_TEST(test_heartbeat_marks_connected);
  RUN_TEST(test_link_lost_after_three_missed_periods);
  RUN_TEST(test_recovers_on_next_heartbeat);
  RUN_TEST(test_survives_millis_rollover);
  return UNITY_END();
}

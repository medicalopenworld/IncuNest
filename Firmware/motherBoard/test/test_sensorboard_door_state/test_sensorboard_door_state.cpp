#include <unity.h>

#include "modules/sensorboard_comm/sb_door_state.h"

void setUp(void) {}
void tearDown(void) {}

void test_no_state_until_first_event(void) {
  SbDoorState s;
  sb_door_state_init(&s);
  bool open = true;
  TEST_ASSERT_FALSE(sb_door_state_get(&s, &open));
  TEST_ASSERT_FALSE(sb_door_state_is_faulty(&s, 0));
}

void test_tracks_open_and_closed(void) {
  SbDoorState s;
  sb_door_state_init(&s);
  bool open = false;

  sb_door_state_note_event(&s, true, 1000);
  TEST_ASSERT_TRUE(sb_door_state_get(&s, &open));
  TEST_ASSERT_TRUE(open);

  sb_door_state_note_event(&s, false, 5000);
  TEST_ASSERT_TRUE(sb_door_state_get(&s, &open));
  TEST_ASSERT_FALSE(open);
}

// El SensorBoard re-afirma el estado cada 30 s aunque no cambie: eso no es
// una transicion y no puede acercar la puerta a declararse averiada.
void test_reassertions_are_not_transitions(void) {
  SbDoorState s;
  sb_door_state_init(&s);
  sb_door_state_note_event(&s, true, 1000);
  for (uint32_t t = 31000; t <= 181000; t += 30000) {
    sb_door_state_note_event(&s, true, t);
  }
  TEST_ASSERT_FALSE(sb_door_state_is_faulty(&s, 181000));
  bool open = false;
  TEST_ASSERT_TRUE(sb_door_state_get(&s, &open));
  TEST_ASSERT_TRUE(open);
}

// Uso normal: abrir y cerrar un par de veces no es sospechoso.
void test_normal_open_close_cycles_are_not_faulty(void) {
  SbDoorState s;
  sb_door_state_init(&s);
  sb_door_state_note_event(&s, false, 0);
  sb_door_state_note_event(&s, true, 10000);
  sb_door_state_note_event(&s, false, 20000);
  sb_door_state_note_event(&s, true, 30000);
  TEST_ASSERT_FALSE(sb_door_state_is_faulty(&s, 30000));
}

// 4 transiciones dentro de la ventana de 60 s: hall sospechoso.
void test_flapping_within_window_is_faulty(void) {
  SbDoorState s;
  sb_door_state_init(&s);
  sb_door_state_note_event(&s, false, 0);
  sb_door_state_note_event(&s, true, 1000);
  sb_door_state_note_event(&s, false, 2000);
  sb_door_state_note_event(&s, true, 3000);
  sb_door_state_note_event(&s, false, 4000);
  TEST_ASSERT_TRUE(sb_door_state_is_faulty(&s, 4000));
}

// Se limpia sola cuando la transicion mas vieja sale de la ventana: la
// alarma no es latching.
void test_fault_clears_when_flapping_stops(void) {
  SbDoorState s;
  sb_door_state_init(&s);
  sb_door_state_note_event(&s, false, 0);
  sb_door_state_note_event(&s, true, 1000);
  sb_door_state_note_event(&s, false, 2000);
  sb_door_state_note_event(&s, true, 3000);
  sb_door_state_note_event(&s, false, 4000);
  TEST_ASSERT_TRUE(sb_door_state_is_faulty(&s, 4000));

  TEST_ASSERT_TRUE(sb_door_state_is_faulty(&s, 1000 + SB_DOOR_FLAP_WINDOW_MS));
  TEST_ASSERT_FALSE(
      sb_door_state_is_faulty(&s, 1000 + SB_DOOR_FLAP_WINDOW_MS + 1));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_no_state_until_first_event);
  RUN_TEST(test_tracks_open_and_closed);
  RUN_TEST(test_reassertions_are_not_transitions);
  RUN_TEST(test_normal_open_close_cycles_are_not_faulty);
  RUN_TEST(test_flapping_within_window_is_faulty);
  RUN_TEST(test_fault_clears_when_flapping_stops);
  return UNITY_END();
}

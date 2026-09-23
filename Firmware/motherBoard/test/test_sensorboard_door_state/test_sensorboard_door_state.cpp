#include <unity.h>

#include "modules/sensorboard_comm/sb_door_state.h"

void setUp(void) {}
void tearDown(void) {}

void test_no_state_until_first_event(void) {
  SbDoorState s;
  sb_door_state_init(&s);
  bool open = true;
  TEST_ASSERT_FALSE(sb_door_state_get(&s, &open));
  TEST_ASSERT_FALSE(sb_door_state_evaluate(&s, 0));
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
  TEST_ASSERT_FALSE(sb_door_state_evaluate(&s, 181000));
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
  TEST_ASSERT_FALSE(sb_door_state_evaluate(&s, 30000));
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
  TEST_ASSERT_TRUE(sb_door_state_evaluate(&s, 4000));
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
  TEST_ASSERT_TRUE(sb_door_state_evaluate(&s, 4000));

  TEST_ASSERT_TRUE(sb_door_state_evaluate(&s, 1000 + SB_DOOR_FLAP_WINDOW_MS));
  TEST_ASSERT_FALSE(
      sb_door_state_evaluate(&s, 1000 + SB_DOOR_FLAP_WINDOW_MS + 1));
}

// Cuatro transiciones fosilizadas resucitaban la alarma 49.7 dias despues, al
// dar la vuelta millis() y volver a caer dentro de la ventana. Con la purga
// estructural el buffer ya esta vacio mucho antes de llegar al vuelco.
void test_old_transitions_do_not_resurrect_after_rollover(void) {
  SbDoorState s;
  sb_door_state_init(&s);
  sb_door_state_note_event(&s, false, 0);
  sb_door_state_note_event(&s, true, 1000);
  sb_door_state_note_event(&s, false, 2000);
  sb_door_state_note_event(&s, true, 3000);
  sb_door_state_note_event(&s, false, 4000);
  TEST_ASSERT_TRUE(sb_door_state_evaluate(&s, 4000));

  // Semanas sin tocar la puerta: la ventana se vacia.
  TEST_ASSERT_FALSE(sb_door_state_evaluate(&s, 0xFFFFFF00u));

  // Y tras el vuelco, now_ms vuelve a recorrer los valores de las
  // transiciones antiguas sin que la alarma reaparezca.
  TEST_ASSERT_FALSE(sb_door_state_evaluate(&s, 4000));
  TEST_ASSERT_FALSE(sb_door_state_evaluate(&s, 60000));
}

// La purga no puede tragarse un flapping real que ocurra a caballo del vuelco.
void test_flapping_across_rollover_is_still_detected(void) {
  SbDoorState s;
  sb_door_state_init(&s);
  const uint32_t base = 0xFFFFF000u;
  sb_door_state_note_event(&s, false, base);
  sb_door_state_note_event(&s, true, base + 1000u);
  sb_door_state_note_event(&s, false, base + 2000u);
  sb_door_state_note_event(&s, true, base + 3000u);
  sb_door_state_note_event(&s, false, base + 4000u);  // ya dio la vuelta
  TEST_ASSERT_TRUE(sb_door_state_evaluate(&s, base + 4000u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_old_transitions_do_not_resurrect_after_rollover);
  RUN_TEST(test_flapping_across_rollover_is_still_detected);
  RUN_TEST(test_no_state_until_first_event);
  RUN_TEST(test_tracks_open_and_closed);
  RUN_TEST(test_reassertions_are_not_transitions);
  RUN_TEST(test_normal_open_close_cycles_are_not_faulty);
  RUN_TEST(test_flapping_within_window_is_faulty);
  RUN_TEST(test_fault_clears_when_flapping_stops);
  return UNITY_END();
}

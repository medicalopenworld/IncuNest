// Alta del paciente vs descarte de una identidad no confirmada (K2).
#include <unity.h>

#include "modules/maintenance/maintenance_seq.h"

void setUp(void) {}
void tearDown(void) {}

void test_no_change_is_not_a_discharge(void) {
  MntSeqDecision d = mnt_seq_decide(7, 7, 0);
  TEST_ASSERT_FALSE(d.changed);
  TEST_ASSERT_FALSE(d.markTerminalPending);
}

void test_first_patient_is_not_a_discharge(void) {
  // 0 -> seq: nadie ha salido.
  MntSeqDecision d = mnt_seq_decide(0, 7, 0);
  TEST_ASSERT_TRUE(d.changed);
  TEST_ASSERT_FALSE(d.markTerminalPending);
}

void test_real_discharge_marks_terminal_clean(void) {
  MntSeqDecision d = mnt_seq_decide(7, 0, 0);
  TEST_ASSERT_TRUE(d.changed);
  TEST_ASSERT_TRUE(d.markTerminalPending);
}

void test_swapping_babies_marks_terminal_clean(void) {
  MntSeqDecision d = mnt_seq_decide(7, 9, 0);
  TEST_ASSERT_TRUE(d.changed);
  TEST_ASSERT_TRUE(d.markTerminalPending);
}

// El caso de K2: el HMI descarta una identidad restaurada que la placa no
// confirma. seq 7 -> 0 NO es un alta.
void test_dropped_identity_is_not_a_discharge(void) {
  MntSeqDecision d = mnt_seq_decide(7, 0, 7);
  TEST_ASSERT_TRUE(d.changed);  // se guarda el 0
  TEST_ASSERT_FALSE(d.markTerminalPending);
}

void test_dropping_another_seq_does_not_hide_a_real_discharge(void) {
  // Se descarto el 3, pero lo que se va ahora es el 7: eso si es un alta.
  MntSeqDecision d = mnt_seq_decide(7, 0, 3);
  TEST_ASSERT_TRUE(d.changed);
  TEST_ASSERT_TRUE(d.markTerminalPending);
}

void test_drop_only_excuses_the_transition_to_nobody(void) {
  // Descartado el 7 y de pronto aparece el 9: no es "se fue sin sustituto".
  MntSeqDecision d = mnt_seq_decide(7, 9, 7);
  TEST_ASSERT_TRUE(d.changed);
  TEST_ASSERT_TRUE(d.markTerminalPending);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_no_change_is_not_a_discharge);
  RUN_TEST(test_first_patient_is_not_a_discharge);
  RUN_TEST(test_real_discharge_marks_terminal_clean);
  RUN_TEST(test_swapping_babies_marks_terminal_clean);
  RUN_TEST(test_dropped_identity_is_not_a_discharge);
  RUN_TEST(test_dropping_another_seq_does_not_hide_a_real_discharge);
  RUN_TEST(test_drop_only_excuses_the_transition_to_nobody);
  return UNITY_END();
}

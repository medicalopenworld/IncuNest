// Contrato de CTRL,PROBE con incunest_afe4490 v0.94 (include/protocol/spo2_probe.h).
//
// Lo que se protege: que el estado nuevo de la libreria (4, ONLY_LED_SATURATING,
// el habitual de "sonda retirada" en esta placa) se reconozca, y que cualquier
// valor desconocido siga cayendo en "sin contacto" y nunca en APPLIED.

#include <unity.h>

#include "protocol/spo2_probe.h"

void setUp(void) {}
void tearDown(void) {}

void test_values_mirror_the_library(void) {
  // Espejo de ProbeState en incunest_afe4490 v0.94: si esto cambia, el cable
  // cambia de significado.
  TEST_ASSERT_EQUAL_INT(0, SPO2_PROBE_DISCONNECTED);
  TEST_ASSERT_EQUAL_INT(1, SPO2_PROBE_OT_HIGH);
  TEST_ASSERT_EQUAL_INT(2, SPO2_PROBE_APPLIED);
  TEST_ASSERT_EQUAL_INT(3, SPO2_PROBE_AMB_SATURATING);
  TEST_ASSERT_EQUAL_INT(4, SPO2_PROBE_ONLY_LED_SATURATING);
}

void test_legacy_names_keep_their_values(void) {
  TEST_ASSERT_EQUAL_INT(1, SPO2_PROBE_NOT_APPLIED);
  TEST_ASSERT_EQUAL_INT(3, SPO2_PROBE_SATURATING);
}

void test_every_known_state_passes_through(void) {
  for (int v = 0; v <= 4; v++) {
    TEST_ASSERT_EQUAL_INT(v, spo2ProbeFromWire(v));
  }
}

void test_state_4_is_recognised_not_collapsed(void) {
  // Hasta la 4.2.1 el 4 se colapsaba a NOT_APPLIED. En pantalla daba igual,
  // pero se perdia la diferencia entre sonda retirada (4) y deslumbrada (3).
  TEST_ASSERT_EQUAL_INT(SPO2_PROBE_ONLY_LED_SATURATING, spo2ProbeFromWire(4));
  TEST_ASSERT_EQUAL_STRING("ONLY_LED_SATURATING",
                           spo2ProbeName(spo2ProbeFromWire(4)));
}

void test_unknown_values_fail_safe_to_not_applied(void) {
  const int raros[] = {-1, 5, 6, 99, 255, 256, -128};
  for (unsigned i = 0; i < sizeof(raros) / sizeof(raros[0]); i++) {
    TEST_ASSERT_EQUAL_INT(SPO2_PROBE_NOT_APPLIED, spo2ProbeFromWire(raros[i]));
  }
}

void test_only_applied_enables_vitals(void) {
  for (int v = -1; v <= 6; v++) {
    const bool esperado = (v == 2);
    TEST_ASSERT_EQUAL(esperado, spo2ProbeIsApplied(spo2ProbeFromWire(v)));
  }
}

void test_names_are_always_printable(void) {
  for (int v = 0; v < 256; v++) {
    const char *n = spo2ProbeName((Spo2ProbeState)v);
    TEST_ASSERT_NOT_NULL(n);
    TEST_ASSERT_TRUE(n[0] != '\0');
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_values_mirror_the_library);
  RUN_TEST(test_legacy_names_keep_their_values);
  RUN_TEST(test_every_known_state_passes_through);
  RUN_TEST(test_state_4_is_recognised_not_collapsed);
  RUN_TEST(test_unknown_values_fail_safe_to_not_applied);
  RUN_TEST(test_only_applied_enables_vitals);
  RUN_TEST(test_names_are_always_printable);
  return UNITY_END();
}

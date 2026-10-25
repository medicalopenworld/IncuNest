// Regresion de ALARM_PHOTO_LAMP_FAULT: lampara ordenada ON que no consume.
//
// Ver modules/control/photo_fault.h para el criterio de umbral, ciclos e
// histeresis, y openspec/changes/mb-photo-current-check para los escenarios.

#include <unity.h>

#include "modules/control/photo_fault.h"

static PhotoFault f;

void setUp(void) { photo_fault_init(&f); }
void tearDown(void) {}

// Alimenta `n` ciclos evaluables con la misma corriente; devuelve el ultimo.
static bool feed(unsigned n, float amps) {
  bool r = photo_fault_present(&f);
  for (unsigned i = 0; i < n; ++i) r = photo_fault_update(&f, true, amps);
  return r;
}

void test_arranca_sin_averia(void) {
  TEST_ASSERT_FALSE(photo_fault_present(&f));
}

// El quinto ciclo declara, y no antes: cubre un muestreo malo aislado.
void test_sin_corriente_se_declara_en_el_ciclo_n_y_no_antes(void) {
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
  TEST_ASSERT_TRUE(photo_fault_update(&f, true, 0.0f));
  TEST_ASSERT_TRUE(photo_fault_present(&f));
}

void test_corriente_normal_nunca_declara(void) {
  TEST_ASSERT_FALSE(feed(100u, 0.45f));
}

// Los ciclos tienen que ser CONSECUTIVOS: uno bueno reinicia la cuenta.
void test_un_ciclo_bueno_reinicia_la_cuenta(void) {
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
  TEST_ASSERT_FALSE(photo_fault_update(&f, true, 0.45f));
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
}

// El umbral es estricto por debajo: justo en PHOTO_FAULT_MIN_A ya responde.
void test_en_el_umbral_exacto_no_cuenta(void) {
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES * 2u, PHOTO_FAULT_MIN_A));
}

// Histeresis: entre MIN y CLEAR la averia declarada se mantiene.
void test_histeresis_mantiene_entre_min_y_clear(void) {
  TEST_ASSERT_TRUE(feed(PHOTO_FAULT_CYCLES, 0.0f));
  TEST_ASSERT_TRUE(feed(10u, 0.06f));
  TEST_ASSERT_TRUE(photo_fault_present(&f));
}

void test_se_retira_al_alcanzar_clear(void) {
  TEST_ASSERT_TRUE(feed(PHOTO_FAULT_CYCLES, 0.0f));
  TEST_ASSERT_FALSE(photo_fault_update(&f, true, PHOTO_FAULT_CLEAR_A));
  TEST_ASSERT_FALSE(photo_fault_present(&f));
}

// Una lectura en la banda de histeresis NO declara una averia nueva.
void test_la_banda_de_histeresis_no_declara(void) {
  TEST_ASSERT_FALSE(feed(100u, 0.06f));
}

// Dejar de ser evaluable (lampara apagada, sensor mudo, test de fabrica)
// retira la averia: no se puede afirmar que la lampara falle.
void test_no_evaluable_retira_la_averia(void) {
  TEST_ASSERT_TRUE(feed(PHOTO_FAULT_CYCLES, 0.0f));
  TEST_ASSERT_FALSE(photo_fault_update(&f, false, 0.0f));
  TEST_ASSERT_FALSE(photo_fault_present(&f));
}

// ... y reinicia la cuenta: la cuenta previa no se hereda al volver.
void test_no_evaluable_reinicia_la_cuenta(void) {
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
  TEST_ASSERT_FALSE(photo_fault_update(&f, false, 0.0f));
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
  TEST_ASSERT_TRUE(photo_fault_update(&f, true, 0.0f));
}

// Con la medida no evaluable el valor de corriente no se mira: un 0 que llega
// durante el asentamiento no puede acumular cuenta.
void test_no_evaluable_ignora_la_corriente(void) {
  for (unsigned i = 0; i < 50u; ++i)
    TEST_ASSERT_FALSE(photo_fault_update(&f, false, 0.0f));
  TEST_ASSERT_FALSE(photo_fault_update(&f, true, 0.0f));
}

// Tras retirarse puede volver a declararse con otra racha completa.
void test_puede_volver_a_declararse(void) {
  TEST_ASSERT_TRUE(feed(PHOTO_FAULT_CYCLES, 0.0f));
  TEST_ASSERT_FALSE(feed(1u, 0.45f));
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
  TEST_ASSERT_TRUE(feed(1u, 0.0f));
}

// Una corriente negativa (shunt invertido, lectura espuria) es "sin
// corriente": no puede colarse por debajo del umbral hacia el lado bueno.
void test_corriente_negativa_cuenta_como_sin_corriente(void) {
  TEST_ASSERT_TRUE(feed(PHOTO_FAULT_CYCLES, -0.3f));
}

// La cuenta satura: dejar la averia declarada horas no desborda el contador.
void test_la_cuenta_no_desborda(void) {
  TEST_ASSERT_TRUE(feed(1000u, 0.0f));
  TEST_ASSERT_FALSE(feed(1u, 0.45f));
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
}

void test_puntero_nulo_no_revienta(void) {
  TEST_ASSERT_FALSE(photo_fault_update(NULL, true, 0.0f));
  TEST_ASSERT_FALSE(photo_fault_present(NULL));
  photo_fault_init(NULL);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_arranca_sin_averia);
  RUN_TEST(test_sin_corriente_se_declara_en_el_ciclo_n_y_no_antes);
  RUN_TEST(test_corriente_normal_nunca_declara);
  RUN_TEST(test_un_ciclo_bueno_reinicia_la_cuenta);
  RUN_TEST(test_en_el_umbral_exacto_no_cuenta);
  RUN_TEST(test_histeresis_mantiene_entre_min_y_clear);
  RUN_TEST(test_se_retira_al_alcanzar_clear);
  RUN_TEST(test_la_banda_de_histeresis_no_declara);
  RUN_TEST(test_no_evaluable_retira_la_averia);
  RUN_TEST(test_no_evaluable_reinicia_la_cuenta);
  RUN_TEST(test_no_evaluable_ignora_la_corriente);
  RUN_TEST(test_puede_volver_a_declararse);
  RUN_TEST(test_corriente_negativa_cuenta_como_sin_corriente);
  RUN_TEST(test_la_cuenta_no_desborda);
  RUN_TEST(test_puntero_nulo_no_revienta);
  return UNITY_END();
}

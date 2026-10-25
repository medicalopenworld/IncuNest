// Regresion de ALARM_PHOTO_LAMP_FAULT: lampara ordenada ON que no consume, o
// que no se puede vigilar.
//
// Ver modules/control/photo_fault.h para el criterio de umbral, ciclos,
// histeresis y ceguera, y openspec/specs/phototherapy-lamp-monitor.

#include <unity.h>

#include "modules/control/photo_fault.h"

static PhotoFault f;

void setUp(void) { photo_fault_init(&f); }
void tearDown(void) {}

// `n` ciclos con muestra nueva y la misma corriente; devuelve el ultimo.
static bool feed(unsigned n, float amps) {
  bool r = photo_fault_present(&f);
  for (unsigned i = 0; i < n; ++i)
    r = photo_fault_update(&f, PHOTO_FAULT_INPUT_SAMPLE, amps);
  return r;
}

// `n` ciclos sin medida utilizable (sensor mudo o muestra rancia).
static bool blind(unsigned n) {
  bool r = photo_fault_present(&f);
  for (unsigned i = 0; i < n; ++i)
    r = photo_fault_update(&f, PHOTO_FAULT_INPUT_NO_SAMPLE, 0.45f);
  return r;
}

static bool inactive(void) {
  return photo_fault_update(&f, PHOTO_FAULT_INPUT_INACTIVE, 0.0f);
}

void test_arranca_sin_averia(void) {
  TEST_ASSERT_FALSE(photo_fault_present(&f));
}

// --- corriente baja ---

void test_sin_corriente_se_declara_en_el_ciclo_n_y_no_antes(void) {
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
  TEST_ASSERT_TRUE(photo_fault_update(&f, PHOTO_FAULT_INPUT_SAMPLE, 0.0f));
  TEST_ASSERT_TRUE(photo_fault_present(&f));
}

void test_corriente_normal_nunca_declara(void) {
  TEST_ASSERT_FALSE(feed(100u, 0.45f));
}

void test_un_ciclo_bueno_reinicia_la_cuenta(void) {
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
  TEST_ASSERT_FALSE(feed(1u, 0.45f));
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
}

void test_en_el_umbral_exacto_no_cuenta(void) {
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES * 2u, PHOTO_FAULT_MIN_A));
}

void test_histeresis_mantiene_entre_min_y_clear(void) {
  TEST_ASSERT_TRUE(feed(PHOTO_FAULT_CYCLES, 0.0f));
  TEST_ASSERT_TRUE(feed(10u, 0.06f));
  TEST_ASSERT_TRUE(photo_fault_present(&f));
}

void test_se_retira_al_alcanzar_clear(void) {
  TEST_ASSERT_TRUE(feed(PHOTO_FAULT_CYCLES, 0.0f));
  TEST_ASSERT_FALSE(feed(1u, PHOTO_FAULT_CLEAR_A));
  TEST_ASSERT_FALSE(photo_fault_present(&f));
}

void test_la_banda_de_histeresis_no_declara(void) {
  TEST_ASSERT_FALSE(feed(100u, 0.06f));
}

void test_puede_volver_a_declararse(void) {
  TEST_ASSERT_TRUE(feed(PHOTO_FAULT_CYCLES, 0.0f));
  TEST_ASSERT_FALSE(feed(1u, 0.45f));
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
  TEST_ASSERT_TRUE(feed(1u, 0.0f));
}

void test_corriente_negativa_cuenta_como_sin_corriente(void) {
  TEST_ASSERT_TRUE(feed(PHOTO_FAULT_CYCLES, -0.3f));
}

void test_la_cuenta_no_desborda(void) {
  TEST_ASSERT_TRUE(feed(1000u, 0.0f));
  TEST_ASSERT_FALSE(feed(1u, 0.45f));
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
}

// --- INACTIVE: apagada / test de fabrica / asentamiento ---

void test_inactivo_retira_la_averia(void) {
  TEST_ASSERT_TRUE(feed(PHOTO_FAULT_CYCLES, 0.0f));
  TEST_ASSERT_FALSE(inactive());
  TEST_ASSERT_FALSE(photo_fault_present(&f));
}

void test_inactivo_reinicia_la_cuenta_de_corriente(void) {
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
  TEST_ASSERT_FALSE(inactive());
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
  TEST_ASSERT_TRUE(feed(1u, 0.0f));
}

void test_inactivo_reinicia_la_cuenta_de_ceguera(void) {
  TEST_ASSERT_FALSE(blind(PHOTO_FAULT_BLIND_CYCLES - 1u));
  TEST_ASSERT_FALSE(inactive());
  TEST_ASSERT_FALSE(blind(PHOTO_FAULT_BLIND_CYCLES - 1u));
  TEST_ASSERT_TRUE(blind(1u));
}

void test_inactivo_ignora_la_corriente(void) {
  for (unsigned i = 0; i < 50u; ++i)
    TEST_ASSERT_FALSE(photo_fault_update(&f, PHOTO_FAULT_INPUT_INACTIVE, 0.0f));
  TEST_ASSERT_FALSE(feed(1u, 0.0f));
}

// --- NO_SAMPLE: sensor mudo o muestra rancia (H1) ---

// Una lectura fallida aislada no retira una averia declarada: sin esto la
// alarma parpadearia cada vez que el bus pierde un sondeo.
void test_una_lectura_fallida_no_retira_la_averia(void) {
  TEST_ASSERT_TRUE(feed(PHOTO_FAULT_CYCLES, 0.0f));
  TEST_ASSERT_TRUE(blind(1u));
  TEST_ASSERT_TRUE(blind(PHOTO_FAULT_BLIND_CYCLES - 2u));
  TEST_ASSERT_TRUE(photo_fault_present(&f));
}

// Tampoco reinicia la cuenta de corriente baja a medias.
void test_ceguera_no_reinicia_la_cuenta_de_corriente(void) {
  TEST_ASSERT_FALSE(feed(PHOTO_FAULT_CYCLES - 1u, 0.0f));
  TEST_ASSERT_FALSE(blind(1u));
  TEST_ASSERT_TRUE(feed(1u, 0.0f));
}

// Sin medida durante N ciclos con la fototerapia ON: se declara. Cubre
// tambien el INA principal AUSENTE desde el arranque (nunca llega una muestra).
void test_sin_medida_N_ciclos_declara_y_no_antes(void) {
  TEST_ASSERT_FALSE(blind(PHOTO_FAULT_BLIND_CYCLES - 1u));
  TEST_ASSERT_TRUE(blind(1u));
  TEST_ASSERT_TRUE(photo_fault_present(&f));
}

// La ceguera tiene que ser CONSECUTIVA: una muestra nueva corta la racha.
void test_una_muestra_nueva_corta_la_racha_de_ceguera(void) {
  TEST_ASSERT_FALSE(blind(PHOTO_FAULT_BLIND_CYCLES - 1u));
  TEST_ASSERT_FALSE(feed(1u, 0.45f));
  TEST_ASSERT_FALSE(blind(PHOTO_FAULT_BLIND_CYCLES - 1u));
  TEST_ASSERT_TRUE(blind(1u));
}

// Declarada por ceguera, se retira cuando vuelve una medida buena...
void test_declarada_por_ceguera_se_retira_con_corriente_buena(void) {
  TEST_ASSERT_TRUE(blind(PHOTO_FAULT_BLIND_CYCLES));
  TEST_ASSERT_FALSE(feed(1u, 0.45f));
  TEST_ASSERT_FALSE(photo_fault_present(&f));
}

// ... pero con la histeresis de siempre: una medida en la banda no la retira.
void test_declarada_por_ceguera_respeta_la_histeresis(void) {
  TEST_ASSERT_TRUE(blind(PHOTO_FAULT_BLIND_CYCLES));
  TEST_ASSERT_TRUE(feed(1u, 0.06f));
  TEST_ASSERT_FALSE(feed(1u, PHOTO_FAULT_CLEAR_A));
}

// ... y apagar la fototerapia la retira aunque el sensor siga mudo.
void test_declarada_por_ceguera_se_retira_al_apagar(void) {
  TEST_ASSERT_TRUE(blind(PHOTO_FAULT_BLIND_CYCLES));
  TEST_ASSERT_FALSE(inactive());
}

void test_la_ceguera_no_desborda(void) {
  TEST_ASSERT_TRUE(blind(1000u));
  TEST_ASSERT_FALSE(feed(1u, 0.45f));
  TEST_ASSERT_FALSE(blind(PHOTO_FAULT_BLIND_CYCLES - 1u));
}

void test_puntero_nulo_no_revienta(void) {
  TEST_ASSERT_FALSE(photo_fault_update(NULL, PHOTO_FAULT_INPUT_SAMPLE, 0.0f));
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
  RUN_TEST(test_puede_volver_a_declararse);
  RUN_TEST(test_corriente_negativa_cuenta_como_sin_corriente);
  RUN_TEST(test_la_cuenta_no_desborda);
  RUN_TEST(test_inactivo_retira_la_averia);
  RUN_TEST(test_inactivo_reinicia_la_cuenta_de_corriente);
  RUN_TEST(test_inactivo_reinicia_la_cuenta_de_ceguera);
  RUN_TEST(test_inactivo_ignora_la_corriente);
  RUN_TEST(test_una_lectura_fallida_no_retira_la_averia);
  RUN_TEST(test_ceguera_no_reinicia_la_cuenta_de_corriente);
  RUN_TEST(test_sin_medida_N_ciclos_declara_y_no_antes);
  RUN_TEST(test_una_muestra_nueva_corta_la_racha_de_ceguera);
  RUN_TEST(test_declarada_por_ceguera_se_retira_con_corriente_buena);
  RUN_TEST(test_declarada_por_ceguera_respeta_la_histeresis);
  RUN_TEST(test_declarada_por_ceguera_se_retira_al_apagar);
  RUN_TEST(test_la_ceguera_no_desborda);
  RUN_TEST(test_puntero_nulo_no_revienta);
  return UNITY_END();
}

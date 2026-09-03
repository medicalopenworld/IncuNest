#include <unity.h>

#include "modules/sensorboard_comm/sb_env_fusion.h"

void setUp(void) {}
void tearDown(void) {}

static SbFusion fuse_temp(bool v0, float t0, bool v1, float t1, bool v2,
                          float t2) {
  const bool valid[3] = {v0, v1, v2};
  const float value[3] = {t0, t1, t2};
  return sb_fuse(valid, value, SB_ENV_TEMP_MIN_C, SB_ENV_TEMP_MAX_C,
                 SB_ENV_MAX_SPREAD_C);
}

// EL CASO QUE MOTIVA TODO ESTO. Con una media simple, un solo sensor pegado a
// 25 C arrastraba la variable del PID hacia abajo: el lazo calentaba para
// compensar y la cabina se iba a 41.5 C con consigna 36, sin que saltara nada
// -- el corte termico y la alarma de desviacion leen esa MISMA media. La
// mediana de tres vota y deja fuera al mentiroso.
void test_un_sensor_sesgado_bajo_no_arrastra_la_medida(void) {
  SbFusion f = fuse_temp(true, 36.0f, true, 36.2f, true, 25.0f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(2, f.used);
  TEST_ASSERT_EQUAL_UINT8(1, f.discarded);
  // 36.1, no 32.4 como daba la media de las tres.
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.1f, f.value);
}

// El caso simetrico. Es benigno (dispararia el corte termico), pero tampoco
// tiene por que contaminar la medida.
void test_un_sensor_sesgado_alto_tampoco_arrastra(void) {
  SbFusion f = fuse_temp(true, 36.0f, true, 36.2f, true, 45.0f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(2, f.used);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.1f, f.value);
}

void test_tres_sensores_de_acuerdo_promedian_los_tres(void) {
  SbFusion f = fuse_temp(true, 36.0f, true, 36.2f, true, 36.4f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(3, f.used);
  TEST_ASSERT_EQUAL_UINT8(0, f.discarded);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.2f, f.value);
  TEST_ASSERT_TRUE(f.has_redundant);
}

// Dos en desacuerdo no se pueden arbitrar: sin tercero que vote, promediar
// seria mezclar una verdad con una mentira. Mejor declarar que no hay medida
// y dejar que salte ALARM_AIR_SENSOR_FAULT.
void test_dos_en_desacuerdo_no_dan_medida(void) {
  SbFusion f = fuse_temp(true, 36.0f, false, 0.0f, true, 25.0f);
  TEST_ASSERT_FALSE(f.valid);
}

void test_dos_de_acuerdo_valen(void) {
  SbFusion f = fuse_temp(true, 36.0f, false, 0.0f, true, 36.4f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(2, f.used);
  TEST_ASSERT_TRUE(f.has_redundant);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.2f, f.value);
}

// Una sola posicion: hay medida y no hay redundancia. Cortar el control
// termico por haber perdido redundancia seria peor que seguir midiendo.
void test_una_sola_posicion_vale_pero_sin_redundancia(void) {
  SbFusion f = fuse_temp(false, 0.0f, true, 36.5f, false, 0.0f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(1, f.used);
  TEST_ASSERT_FALSE(f.has_redundant);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.5f, f.value);
}

void test_sin_posiciones_validas_no_hay_medida(void) {
  SbFusion f = fuse_temp(false, 0.0f, false, 0.0f, false, 0.0f);
  TEST_ASSERT_FALSE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(0, f.used);
}

// El gate de cabina es mas estrecho que el de datasheet: una incubadora a
// 8 C no esta midiendo su cabina, esta midiendo otra cosa.
void test_valores_fuera_del_rango_de_cabina_se_descartan(void) {
  SbFusion f = fuse_temp(true, 8.0f, true, 36.0f, true, 36.2f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(2, f.used);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.1f, f.value);

  // Y si TODAS estan fuera de rango, no hay medida.
  SbFusion g = fuse_temp(true, 8.0f, true, 9.0f, true, 70.0f);
  TEST_ASSERT_FALSE(g.valid);
}

// Tres completamente dispersos: la mediana sobrevive sola. Hay medida, pero
// sin redundancia y con dos descartes, que es lo que hay que reportar.
void test_tres_dispersos_dejan_solo_la_mediana(void) {
  SbFusion f = fuse_temp(true, 20.0f, true, 36.0f, true, 48.0f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(1, f.used);
  TEST_ASSERT_EQUAL_UINT8(2, f.discarded);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.0f, f.value);
  TEST_ASSERT_FALSE(f.has_redundant);
}

void test_humedad_usa_su_propia_dispersion(void) {
  const bool valid[3] = {true, true, true};
  const float value[3] = {45.0f, 47.0f, 80.0f};
  SbFusion f = sb_fuse(valid, value, 0.0f, 100.0f, SB_ENV_MAX_SPREAD_RH);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(2, f.used);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 46.0f, f.value);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_un_sensor_sesgado_bajo_no_arrastra_la_medida);
  RUN_TEST(test_un_sensor_sesgado_alto_tampoco_arrastra);
  RUN_TEST(test_tres_sensores_de_acuerdo_promedian_los_tres);
  RUN_TEST(test_dos_en_desacuerdo_no_dan_medida);
  RUN_TEST(test_dos_de_acuerdo_valen);
  RUN_TEST(test_una_sola_posicion_vale_pero_sin_redundancia);
  RUN_TEST(test_sin_posiciones_validas_no_hay_medida);
  RUN_TEST(test_valores_fuera_del_rango_de_cabina_se_descartan);
  RUN_TEST(test_tres_dispersos_dejan_solo_la_mediana);
  RUN_TEST(test_humedad_usa_su_propia_dispersion);
  return UNITY_END();
}

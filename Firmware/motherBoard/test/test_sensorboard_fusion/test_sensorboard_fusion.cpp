#include <unity.h>

#include "modules/sensorboard_comm/sb_env_fusion.h"

void setUp(void) {}
void tearDown(void) {}

static SbFusion fuse_temp(bool v0, float t0, bool v1, float t1, bool v2,
                          float t2) {
  const bool valid[3] = {v0, v1, v2};
  const float value[3] = {t0, t1, t2};
  return sb_fuse(valid, value, SB_ENV_TEMP_MIN_C, SB_ENV_TEMP_MAX_C);
}

// LA RAZON DE USAR MEDIANA Y NO MEDIA. De la variable que sale de aqui comen
// el PID, el corte termico y la alarma de desviacion. Con la media, un sensor
// pegado a 25 C y consigna 36 llevaba la cabina real a 41.5 C sin que saltara
// ninguna de las tres, porque todas leen ese mismo numero. La mediana de tres
// siempre es una lectura real de un sensor.
void test_mediana_ignora_un_sensor_sesgado_bajo(void) {
  SbFusion f = fuse_temp(true, 36.0f, true, 36.2f, true, 25.0f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(3, f.used);
  // 36.0 (la del medio), no 32.4 como daba la media.
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.0f, f.value);
}

void test_mediana_ignora_un_sensor_sesgado_alto(void) {
  SbFusion f = fuse_temp(true, 36.0f, true, 36.2f, true, 45.0f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.2f, f.value);
}

void test_tres_de_acuerdo_dan_la_del_medio(void) {
  SbFusion f = fuse_temp(true, 36.0f, true, 36.2f, true, 36.4f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(3, f.used);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.2f, f.value);
}

// El orden en que llegan las posiciones no puede cambiar el resultado.
void test_la_mediana_no_depende_del_orden(void) {
  const float a = fuse_temp(true, 25.0f, true, 36.0f, true, 36.2f).value;
  const float b = fuse_temp(true, 36.2f, true, 25.0f, true, 36.0f).value;
  const float c = fuse_temp(true, 36.0f, true, 36.2f, true, 25.0f).value;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, a, b);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, a, c);
}

// Con dos no hay tercero que arbitre: se promedian, y decidir cual descartar
// queda para el cribado que la motherboard hara mas adelante.
void test_dos_posiciones_se_promedian(void) {
  SbFusion f = fuse_temp(true, 36.0f, false, 0.0f, true, 36.4f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(2, f.used);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.2f, f.value);
}

// Sin cribado: dos que discrepan siguen dando medida. Es una decision
// consciente -- el cribado futuro es quien debera detectarlo.
void test_dos_en_desacuerdo_siguen_dando_medida(void) {
  SbFusion f = fuse_temp(true, 36.0f, false, 0.0f, true, 25.0f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(2, f.used);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 30.5f, f.value);
}

void test_una_sola_posicion_vale(void) {
  SbFusion f = fuse_temp(false, 0.0f, true, 36.5f, false, 0.0f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(1, f.used);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.5f, f.value);
}

// Sin ninguna lectura no hay medida: el llamante no refresca el sello y salta
// ALARM_AIR_SENSOR_FAULT, igual que con un STS35 averiado.
void test_sin_posiciones_validas_no_hay_medida(void) {
  SbFusion f = fuse_temp(false, 0.0f, false, 0.0f, false, 0.0f);
  TEST_ASSERT_FALSE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(0, f.used);
}

// Descartar un valor imposible no es cribado: es lo que ya hacia el camino
// I2C. Una incubadora a 8 C no esta midiendo su cabina.
void test_valores_imposibles_no_cuentan(void) {
  SbFusion f = fuse_temp(true, 8.0f, true, 36.0f, true, 36.2f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_EQUAL_UINT8(2, f.used);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 36.1f, f.value);

  SbFusion g = fuse_temp(true, 8.0f, true, 9.0f, true, 70.0f);
  TEST_ASSERT_FALSE(g.valid);
}

void test_humedad_usa_su_propio_rango(void) {
  const bool valid[3] = {true, true, true};
  const float value[3] = {45.0f, 47.0f, 80.0f};
  SbFusion f = sb_fuse(valid, value, 0.0f, 100.0f);
  TEST_ASSERT_TRUE(f.valid);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 47.0f, f.value);  // mediana
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_mediana_ignora_un_sensor_sesgado_bajo);
  RUN_TEST(test_mediana_ignora_un_sensor_sesgado_alto);
  RUN_TEST(test_tres_de_acuerdo_dan_la_del_medio);
  RUN_TEST(test_la_mediana_no_depende_del_orden);
  RUN_TEST(test_dos_posiciones_se_promedian);
  RUN_TEST(test_dos_en_desacuerdo_siguen_dando_medida);
  RUN_TEST(test_una_sola_posicion_vale);
  RUN_TEST(test_sin_posiciones_validas_no_hay_medida);
  RUN_TEST(test_valores_imposibles_no_cuentan);
  RUN_TEST(test_humedad_usa_su_propio_rango);
  return UNITY_END();
}

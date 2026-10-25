// Reglas puras de la configuracion remota (openspec: mb-remote-config).
//
// Cubre lo que no necesita hardware: la lista blanca, la regla de cada uno de
// los nueve parametros, la lectura estricta de texto y de JSON y el formato del
// eco. Que el valor llegue a in3, a NVS o a un LEDC es verificacion manual (ver
// el spec): no hay entorno de test para eso y aqui no se finge.

#include <math.h>
#include <string.h>

#include <unity.h>

#include "modules/config/runtime_config_rules.h"

void setUp(void) {}
void tearDown(void) {}

static const float NAN_F = NAN;
static const float INF_F = INFINITY;
static const float SENTINEL = -12345.0f;

// Valida y devuelve el estado; deja el valor de salida en *out (SENTINEL si no
// se toco).
static RuntimeConfigStatus run(RuntimeConfigId id, float in, float *out) {
  *out = SENTINEL;
  return runtime_config_validate(id, in, out);
}

// --- lista blanca ----------------------------------------------------------

static void test_los_nueve_nombres_resuelven_a_su_id(void) {
  const char *names[RCFG_COUNT] = {
      "fan_supply_pwm", "fan_ctl_pwm", "fan_pid_en",
      "heater_amps",    "air_tmax",    "skin_tmax",
      "gprs_act",       "gprs_photo",  "gprs_stby"};
  for (int i = 0; i < RCFG_COUNT; ++i) {
    RuntimeConfigId id = RCFG_COUNT;
    TEST_ASSERT_TRUE_MESSAGE(runtime_config_lookup(names[i], &id), names[i]);
    TEST_ASSERT_EQUAL_INT(i, (int)id);
    TEST_ASSERT_EQUAL_STRING(names[i], runtime_config_name((RuntimeConfigId)i));
  }
}

static void test_lo_que_no_esta_en_la_lista_blanca_no_resuelve(void) {
  // serial, reference_temp y set_time se quedan fuera A PROPOSITO.
  const char *fuera[] = {"serial", "reference_temp", "set_time", "BABY_WIPE",
                         "foo",    "",               "air_tmax ",  " air_tmax",
                         "air",    "air_tmaxx",      "fw_title"};
  for (unsigned i = 0; i < sizeof(fuera) / sizeof(fuera[0]); ++i) {
    RuntimeConfigId id = RCFG_AIR_TMAX;
    TEST_ASSERT_FALSE_MESSAGE(runtime_config_lookup(fuera[i], &id), fuera[i]);
    TEST_ASSERT_EQUAL_INT_MESSAGE(RCFG_AIR_TMAX, (int)id,
                                  "un fallo no debe tocar la salida");
  }
  RuntimeConfigId id;
  TEST_ASSERT_FALSE(runtime_config_lookup(nullptr, &id));
}

static void test_la_consola_usa_mayusculas(void) {
  RuntimeConfigId id = RCFG_COUNT;
  TEST_ASSERT_TRUE(runtime_config_lookup("AIR_TMAX", &id));
  TEST_ASSERT_EQUAL_INT(RCFG_AIR_TMAX, (int)id);
  TEST_ASSERT_TRUE(runtime_config_lookup("Gprs_Stby", &id));
  TEST_ASSERT_EQUAL_INT(RCFG_GPRS_STBY, (int)id);
}

static void test_nombre_de_id_invalido_es_nulo(void) {
  TEST_ASSERT_NULL(runtime_config_name(RCFG_COUNT));
  TEST_ASSERT_NULL(runtime_config_name((RuntimeConfigId)200));
}

// --- PWM -------------------------------------------------------------------

static void test_pwm_en_rango_se_acepta_sin_cambios(void) {
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_FAN_CTL_PWM, 130.0f, &out));
  TEST_ASSERT_EQUAL_FLOAT(130.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_FAN_SUPPLY_PWM, 1.0f, &out));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_FAN_SUPPLY_PWM, 255.0f, &out));
  TEST_ASSERT_EQUAL_FLOAT(255.0f, out);
}

static void test_pwm_fuera_de_rango_se_rechaza(void) {
  const float malos[] = {0.0f, -1.0f, 256.0f, 300.0f, 100000.0f};
  const RuntimeConfigId ids[] = {RCFG_FAN_SUPPLY_PWM, RCFG_FAN_CTL_PWM};
  for (unsigned i = 0; i < 2; ++i) {
    for (unsigned j = 0; j < sizeof(malos) / sizeof(malos[0]); ++j) {
      float out;
      TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run(ids[i], malos[j], &out));
      TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
    }
  }
}

static void test_entero_con_parte_decimal_se_rechaza(void) {
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_INTEGER, run(RCFG_FAN_CTL_PWM, 130.5f, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_INTEGER, run(RCFG_GPRS_ACT, 60.25f, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
}

// --- periodos --------------------------------------------------------------

static void test_periodos_limites(void) {
  const RuntimeConfigId ids[] = {RCFG_GPRS_ACT, RCFG_GPRS_PHOTO,
                                 RCFG_GPRS_STBY};
  for (unsigned i = 0; i < 3; ++i) {
    float out;
    TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run(ids[i], 9.0f, &out));
    TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
    TEST_ASSERT_EQUAL_INT(RCFG_OK, run(ids[i], 10.0f, &out));
    TEST_ASSERT_EQUAL_FLOAT(10.0f, out);
    TEST_ASSERT_EQUAL_INT(RCFG_OK, run(ids[i], 86400.0f, &out));
    TEST_ASSERT_EQUAL_FLOAT(86400.0f, out);
    TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run(ids[i], 86401.0f, &out));
    TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
  }
}

static void test_periodo_cero_o_negativo_se_rechaza(void) {
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run(RCFG_GPRS_ACT, 0.0f, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run(RCFG_GPRS_PHOTO, -60.0f, &out));
}

// --- no finitos ------------------------------------------------------------

static void test_nan_e_inf_se_rechazan_en_los_nueve(void) {
  for (int i = 0; i < RCFG_COUNT; ++i) {
    float out;
    TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_FINITE,
                          run((RuntimeConfigId)i, NAN_F, &out));
    TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
    TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_FINITE,
                          run((RuntimeConfigId)i, INF_F, &out));
    TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
    TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_FINITE,
                          run((RuntimeConfigId)i, -INF_F, &out));
    TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
  }
}

// --- cortes termicos: se RECORTAN, no se rechazan --------------------------

static void test_air_tmax_se_recorta(void) {
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_OK_CLAMPED, run(RCFG_AIR_TMAX, 45.0f, &out));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_OK_CLAMPED, run(RCFG_AIR_TMAX, 30.0f, &out));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 34.0f, out);
}

static void test_skin_tmax_se_recorta(void) {
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_OK_CLAMPED, run(RCFG_SKIN_TMAX, 45.0f, &out));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_OK_CLAMPED, run(RCFG_SKIN_TMAX, -5.0f, &out));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 34.0f, out);
}

static void test_tmax_en_rango_no_se_marca_como_recortada(void) {
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_AIR_TMAX, 38.0f, &out));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 38.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_SKIN_TMAX, 37.5f, &out));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 37.5f, out);
  // Los extremos del rango son valores validos, no recortes.
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_AIR_TMAX, 40.0f, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_AIR_TMAX, 34.0f, &out));
}

// --- fan_pid_en ------------------------------------------------------------

static void test_fan_pid_en_solo_acepta_0_y_1(void) {
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_FAN_PID_EN, 0.0f, &out));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_FAN_PID_EN, 1.0f, &out));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_BOOL, run(RCFG_FAN_PID_EN, 2.0f, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_BOOL, run(RCFG_FAN_PID_EN, 0.5f, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_BOOL, run(RCFG_FAN_PID_EN, -1.0f, &out));
}

// --- heater_amps -----------------------------------------------------------

static void test_heater_amps_limites(void) {
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run(RCFG_HEATER_AMPS, 0.0f, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run(RCFG_HEATER_AMPS, 0.99f, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_HEATER_AMPS, 1.0f, &out));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_HEATER_AMPS, 9.5f, &out));
  TEST_ASSERT_EQUAL_FLOAT(9.5f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run(RCFG_HEATER_AMPS, 12.0f, &out));
  TEST_ASSERT_EQUAL_FLOAT(12.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run(RCFG_HEATER_AMPS, 12.01f, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
}

// --- estado ----------------------------------------------------------------

static void test_aceptado_solo_para_ok_y_recortado(void) {
  TEST_ASSERT_TRUE(runtime_config_status_accepted(RCFG_OK));
  TEST_ASSERT_TRUE(runtime_config_status_accepted(RCFG_OK_CLAMPED));
  TEST_ASSERT_FALSE(runtime_config_status_accepted(RCFG_REJ_NOT_FINITE));
  TEST_ASSERT_FALSE(runtime_config_status_accepted(RCFG_REJ_RANGE));
  TEST_ASSERT_FALSE(runtime_config_status_accepted(RCFG_REJ_NOT_INTEGER));
  TEST_ASSERT_FALSE(runtime_config_status_accepted(RCFG_REJ_NOT_BOOL));
}

static void test_cada_estado_tiene_texto_propio(void) {
  const RuntimeConfigStatus all[] = {RCFG_OK,
                                     RCFG_OK_CLAMPED,
                                     RCFG_REJ_NOT_FINITE,
                                     RCFG_REJ_RANGE,
                                     RCFG_REJ_NOT_INTEGER,
                                     RCFG_REJ_NOT_BOOL};
  for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
    const char *t = runtime_config_status_text(all[i]);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_TRUE(strlen(t) > 0);
    for (unsigned j = 0; j < i; ++j) {
      TEST_ASSERT_NOT_EQUAL(0, strcmp(t, runtime_config_status_text(all[j])));
    }
  }
}

// --- texto -----------------------------------------------------------------

static void test_parse_numero_valido(void) {
  float out = SENTINEL;
  TEST_ASSERT_TRUE(runtime_config_parse_number("130", &out));
  TEST_ASSERT_EQUAL_FLOAT(130.0f, out);
  TEST_ASSERT_TRUE(runtime_config_parse_number(" 38.5 ", &out));
  TEST_ASSERT_EQUAL_FLOAT(38.5f, out);
  TEST_ASSERT_TRUE(runtime_config_parse_number("-4", &out));
  TEST_ASSERT_EQUAL_FLOAT(-4.0f, out);
}

static void test_parse_numero_invalido(void) {
  const char *malos[] = {"abc", "", "   ", "12x", "1e999", "nan", "inf",
                         "1,5", "0x10", "--1"};
  for (unsigned i = 0; i < sizeof(malos) / sizeof(malos[0]); ++i) {
    float out = SENTINEL;
    TEST_ASSERT_FALSE_MESSAGE(runtime_config_parse_number(malos[i], &out),
                              malos[i]);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(SENTINEL, out, malos[i]);
  }
  float out = SENTINEL;
  TEST_ASSERT_FALSE(runtime_config_parse_number(nullptr, &out));
}

// --- JSON ------------------------------------------------------------------

static bool from_json(const char *json, float *out) {
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) {
    return false;
  }
  return runtime_config_json_number(doc["v"], out);
}

static void test_json_acepta_numeros_y_booleanos(void) {
  float out = SENTINEL;
  TEST_ASSERT_TRUE(from_json("{\"v\":38}", &out));
  TEST_ASSERT_EQUAL_FLOAT(38.0f, out);
  TEST_ASSERT_TRUE(from_json("{\"v\":38.5}", &out));
  TEST_ASSERT_EQUAL_FLOAT(38.5f, out);
  TEST_ASSERT_TRUE(from_json("{\"v\":true}", &out));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, out);
  TEST_ASSERT_TRUE(from_json("{\"v\":false}", &out));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, out);
}

static void test_json_rechaza_el_resto_de_tipos(void) {
  const char *malos[] = {"{\"v\":\"38\"}", "{\"v\":null}", "{\"v\":[1]}",
                         "{\"v\":{}}",     "{\"w\":1}",    "{\"v\":\"\"}"};
  for (unsigned i = 0; i < sizeof(malos) / sizeof(malos[0]); ++i) {
    float out = SENTINEL;
    TEST_ASSERT_FALSE_MESSAGE(from_json(malos[i], &out), malos[i]);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(SENTINEL, out, malos[i]);
  }
}

// --- eco -------------------------------------------------------------------

static void fill_defaults(float v[RCFG_COUNT]) {
  v[RCFG_FAN_SUPPLY_PWM] = 255.0f;
  v[RCFG_FAN_CTL_PWM] = 130.0f;
  v[RCFG_FAN_PID_EN] = 1.0f;
  v[RCFG_HEATER_AMPS] = 9.5f;
  v[RCFG_AIR_TMAX] = 38.0f;
  v[RCFG_SKIN_TMAX] = 37.5f;
  v[RCFG_GPRS_ACT] = 60.0f;
  v[RCFG_GPRS_PHOTO] = 180.0f;
  v[RCFG_GPRS_STBY] = 3600.0f;
}

static void test_eco_solo_lleva_las_claves_pendientes(void) {
  float v[RCFG_COUNT];
  fill_defaults(v);
  char buf[RCFG_ECHO_JSON_MAX];
  const size_t n = runtime_config_format_echo(
      v, RCFG_MASK(RCFG_AIR_TMAX) | RCFG_MASK(RCFG_GPRS_ACT), buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL_UINT(strlen(buf), n);

  StaticJsonDocument<256> doc;
  TEST_ASSERT_EQUAL_INT(DeserializationError::Ok,
                        deserializeJson(doc, buf).code());
  TEST_ASSERT_EQUAL_UINT(2, doc.size());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 38.0f, doc["air_tmax"].as<float>());
  TEST_ASSERT_EQUAL_INT(60, doc["gprs_act"].as<int>());
}

static void test_eco_de_fan_pid_en_es_booleano(void) {
  float v[RCFG_COUNT];
  fill_defaults(v);
  char buf[RCFG_ECHO_JSON_MAX];
  TEST_ASSERT_TRUE(runtime_config_format_echo(v, RCFG_MASK(RCFG_FAN_PID_EN),
                                              buf, sizeof(buf)) > 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"fan_pid_en\":true"));
  v[RCFG_FAN_PID_EN] = 0.0f;
  TEST_ASSERT_TRUE(runtime_config_format_echo(v, RCFG_MASK(RCFG_FAN_PID_EN),
                                              buf, sizeof(buf)) > 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"fan_pid_en\":false"));
}

static void test_eco_completo_a_valores_maximos_cabe(void) {
  float v[RCFG_COUNT];
  v[RCFG_FAN_SUPPLY_PWM] = 255.0f;
  v[RCFG_FAN_CTL_PWM] = 255.0f;
  v[RCFG_FAN_PID_EN] = 0.0f;  // "false" es la rama mas larga
  v[RCFG_HEATER_AMPS] = 12.0f;
  v[RCFG_AIR_TMAX] = 40.0f;
  v[RCFG_SKIN_TMAX] = 40.0f;
  v[RCFG_GPRS_ACT] = 86400.0f;
  v[RCFG_GPRS_PHOTO] = 86400.0f;
  v[RCFG_GPRS_STBY] = 86400.0f;
  char buf[RCFG_ECHO_JSON_MAX];
  const size_t n =
      runtime_config_format_echo(v, RCFG_MASK_ALL, buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_TRUE(n < sizeof(buf));

  StaticJsonDocument<256> doc;
  TEST_ASSERT_EQUAL_INT(DeserializationError::Ok,
                        deserializeJson(doc, buf).code());
  TEST_ASSERT_EQUAL_UINT(RCFG_COUNT, doc.size());
  TEST_ASSERT_EQUAL_INT(86400, doc["gprs_stby"].as<int>());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, doc["heater_amps"].as<float>());
}

static void test_eco_con_mascara_vacia_o_buffer_corto_devuelve_cero(void) {
  float v[RCFG_COUNT];
  fill_defaults(v);
  char buf[RCFG_ECHO_JSON_MAX];
  TEST_ASSERT_EQUAL_UINT(0, runtime_config_format_echo(v, 0, buf, sizeof(buf)));
  char corto[16];
  TEST_ASSERT_EQUAL_UINT(
      0, runtime_config_format_echo(v, RCFG_MASK_ALL, corto, sizeof(corto)));
  TEST_ASSERT_EQUAL_UINT(0, runtime_config_format_echo(v, RCFG_MASK_ALL, buf, 0));
}

static void test_eco_ignora_bits_fuera_de_los_nueve(void) {
  float v[RCFG_COUNT];
  fill_defaults(v);
  char buf[RCFG_ECHO_JSON_MAX];
  TEST_ASSERT_EQUAL_UINT(
      0, runtime_config_format_echo(v, (uint16_t)0xFE00, buf, sizeof(buf)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_los_nueve_nombres_resuelven_a_su_id);
  RUN_TEST(test_lo_que_no_esta_en_la_lista_blanca_no_resuelve);
  RUN_TEST(test_la_consola_usa_mayusculas);
  RUN_TEST(test_nombre_de_id_invalido_es_nulo);
  RUN_TEST(test_pwm_en_rango_se_acepta_sin_cambios);
  RUN_TEST(test_pwm_fuera_de_rango_se_rechaza);
  RUN_TEST(test_entero_con_parte_decimal_se_rechaza);
  RUN_TEST(test_periodos_limites);
  RUN_TEST(test_periodo_cero_o_negativo_se_rechaza);
  RUN_TEST(test_nan_e_inf_se_rechazan_en_los_nueve);
  RUN_TEST(test_air_tmax_se_recorta);
  RUN_TEST(test_skin_tmax_se_recorta);
  RUN_TEST(test_tmax_en_rango_no_se_marca_como_recortada);
  RUN_TEST(test_fan_pid_en_solo_acepta_0_y_1);
  RUN_TEST(test_heater_amps_limites);
  RUN_TEST(test_aceptado_solo_para_ok_y_recortado);
  RUN_TEST(test_cada_estado_tiene_texto_propio);
  RUN_TEST(test_parse_numero_valido);
  RUN_TEST(test_parse_numero_invalido);
  RUN_TEST(test_json_acepta_numeros_y_booleanos);
  RUN_TEST(test_json_rechaza_el_resto_de_tipos);
  RUN_TEST(test_eco_solo_lleva_las_claves_pendientes);
  RUN_TEST(test_eco_de_fan_pid_en_es_booleano);
  RUN_TEST(test_eco_completo_a_valores_maximos_cabe);
  RUN_TEST(test_eco_con_mascara_vacia_o_buffer_corto_devuelve_cero);
  RUN_TEST(test_eco_ignora_bits_fuera_de_los_nueve);
  return UNITY_END();
}

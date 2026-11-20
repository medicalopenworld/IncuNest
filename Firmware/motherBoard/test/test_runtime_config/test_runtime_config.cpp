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

static RuntimeConfigStatus run_strict(RuntimeConfigId id, float in,
                                      float *out) {
  *out = SENTINEL;
  return runtime_config_validate_strict(id, in, out);
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

static void test_la_web_y_thingsboard_son_sensibles_a_mayusculas(void) {
  RuntimeConfigId id = RCFG_COUNT;
  TEST_ASSERT_FALSE(runtime_config_lookup("AIR_TMAX", &id));
  TEST_ASSERT_FALSE(runtime_config_lookup("Gprs_Stby", &id));
  TEST_ASSERT_EQUAL_INT(RCFG_COUNT, (int)id);
}

static void test_la_consola_acepta_solo_las_mayusculas_exactas(void) {
  RuntimeConfigId id = RCFG_COUNT;
  TEST_ASSERT_TRUE(runtime_config_lookup_console("AIR_TMAX", &id));
  TEST_ASSERT_EQUAL_INT(RCFG_AIR_TMAX, (int)id);
  TEST_ASSERT_TRUE(runtime_config_lookup_console("GPRS_STBY", &id));
  TEST_ASSERT_EQUAL_INT(RCFG_GPRS_STBY, (int)id);
  TEST_ASSERT_TRUE(runtime_config_lookup_console("FAN_SUPPLY_PWM", &id));
  TEST_ASSERT_EQUAL_INT(RCFG_FAN_SUPPLY_PWM, (int)id);
  id = RCFG_COUNT;
  TEST_ASSERT_FALSE(runtime_config_lookup_console("air_tmax", &id));
  TEST_ASSERT_FALSE(runtime_config_lookup_console("Air_Tmax", &id));
  TEST_ASSERT_FALSE(runtime_config_lookup_console("SERIAL", &id));
  TEST_ASSERT_FALSE(runtime_config_lookup_console("BABY_WIPE", &id));
  TEST_ASSERT_FALSE(runtime_config_lookup_console("", &id));
  TEST_ASSERT_FALSE(runtime_config_lookup_console(nullptr, &id));
  TEST_ASSERT_EQUAL_INT(RCFG_COUNT, (int)id);
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
  TEST_ASSERT_FALSE(runtime_config_status_accepted(RCFG_REJ_ABOVE_FACTORY));
  TEST_ASSERT_FALSE(runtime_config_status_accepted(RCFG_REJ_BELOW_SETPOINT));
  TEST_ASSERT_FALSE(runtime_config_status_accepted(RCFG_REJ_NO_TACHOMETER));
  TEST_ASSERT_FALSE(runtime_config_status_accepted(RCFG_REJ_HEATER_ACTING));
  TEST_ASSERT_FALSE(runtime_config_status_accepted(RCFG_REJ_NOT_ALLOWED));
  TEST_ASSERT_FALSE(runtime_config_status_accepted(RCFG_REJ_WRONG_TYPE));
}

static void test_cada_estado_tiene_texto_propio(void) {
  const RuntimeConfigStatus all[] = {RCFG_OK,
                                     RCFG_OK_CLAMPED,
                                     RCFG_REJ_NOT_FINITE,
                                     RCFG_REJ_RANGE,
                                     RCFG_REJ_NOT_INTEGER,
                                     RCFG_REJ_NOT_BOOL,
                                     RCFG_REJ_ABOVE_FACTORY,
                                     RCFG_REJ_BELOW_SETPOINT,
                                     RCFG_REJ_NO_TACHOMETER,
                                     RCFG_REJ_HEATER_ACTING,
                                     RCFG_REJ_NOT_ALLOWED,
                                     RCFG_REJ_WRONG_TYPE};
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

static bool from_json(const char *json, float *out, bool allow_bool = true) {
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) {
    return false;
  }
  return runtime_config_json_number(doc["v"], allow_bool, out);
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

static void test_json_booleano_solo_si_se_permite(void) {
  // Solo fan_pid_en admite true/false; en el resto un booleano es un error de
  // tipo (un true en gprs_act NO es un periodo de 1 s).
  float out = SENTINEL;
  TEST_ASSERT_FALSE(from_json("{\"v\":true}", &out, false));
  TEST_ASSERT_FALSE(from_json("{\"v\":false}", &out, false));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
  TEST_ASSERT_TRUE(from_json("{\"v\":1}", &out, false));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, out);
  TEST_ASSERT_TRUE(from_json("{\"v\":38.5}", &out, false));
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
      v, RCFG_MASK(RCFG_AIR_TMAX) | RCFG_MASK(RCFG_GPRS_ACT), nullptr, nullptr,
      buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL_UINT(strlen(buf), n);

  StaticJsonDocument<512> doc;
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
                                              nullptr, nullptr, buf,
                                              sizeof(buf)) > 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"fan_pid_en\":true"));
  v[RCFG_FAN_PID_EN] = 0.0f;
  TEST_ASSERT_TRUE(runtime_config_format_echo(v, RCFG_MASK(RCFG_FAN_PID_EN),
                                              nullptr, nullptr, buf,
                                              sizeof(buf)) > 0);
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
      runtime_config_format_echo(v, RCFG_MASK_ALL, nullptr, nullptr, buf,
                                 sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_TRUE(n < sizeof(buf));

  StaticJsonDocument<512> doc;
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
  TEST_ASSERT_EQUAL_UINT(0, runtime_config_format_echo(v, 0, nullptr, nullptr, buf, sizeof(buf)));
  char corto[16];
  TEST_ASSERT_EQUAL_UINT(
      0, runtime_config_format_echo(v, RCFG_MASK_ALL, nullptr, nullptr, corto,
                                 sizeof(corto)));
  TEST_ASSERT_EQUAL_UINT(0, runtime_config_format_echo(v, RCFG_MASK_ALL, nullptr, nullptr, buf, 0));
}

static void test_eco_ignora_bits_fuera_de_los_nueve(void) {
  float v[RCFG_COUNT];
  fill_defaults(v);
  char buf[RCFG_ECHO_JSON_MAX];
  TEST_ASSERT_EQUAL_UINT(
      0, runtime_config_format_echo(v, (uint16_t)0xFE00, nullptr, nullptr, buf,
                                 sizeof(buf)));
}

// ===========================================================================
//  Revision de seguridad: MQTT va en claro, asi que lo remoto se acota.
// ===========================================================================

// --- lista blanca remota ---------------------------------------------------

static void test_con_el_flag_a_0_solo_los_periodos_son_remotos(void) {
  RuntimeConfigId keys[RCFG_COUNT];
  const size_t n = runtime_config_remote_keys(false, keys);
  TEST_ASSERT_EQUAL_UINT(3, n);
  TEST_ASSERT_EQUAL_INT(RCFG_GPRS_ACT, (int)keys[0]);
  TEST_ASSERT_EQUAL_INT(RCFG_GPRS_PHOTO, (int)keys[1]);
  TEST_ASSERT_EQUAL_INT(RCFG_GPRS_STBY, (int)keys[2]);
  for (int i = 0; i < RCFG_COUNT; ++i) {
    const bool periodo = i == RCFG_GPRS_ACT || i == RCFG_GPRS_PHOTO ||
                         i == RCFG_GPRS_STBY;
    TEST_ASSERT_EQUAL_INT(periodo, runtime_config_remote_allowed(
                                       (RuntimeConfigId)i, false));
  }
}

static void test_con_el_flag_a_1_son_remotos_los_nueve(void) {
  RuntimeConfigId keys[RCFG_COUNT];
  TEST_ASSERT_EQUAL_UINT(RCFG_COUNT, runtime_config_remote_keys(true, keys));
  for (int i = 0; i < RCFG_COUNT; ++i) {
    TEST_ASSERT_TRUE(runtime_config_remote_allowed((RuntimeConfigId)i, true));
  }
}

static void test_seguridad_son_los_seis_que_no_son_periodos(void) {
  TEST_ASSERT_TRUE(runtime_config_is_safety(RCFG_AIR_TMAX));
  TEST_ASSERT_TRUE(runtime_config_is_safety(RCFG_SKIN_TMAX));
  TEST_ASSERT_TRUE(runtime_config_is_safety(RCFG_HEATER_AMPS));
  TEST_ASSERT_TRUE(runtime_config_is_safety(RCFG_FAN_SUPPLY_PWM));
  TEST_ASSERT_TRUE(runtime_config_is_safety(RCFG_FAN_CTL_PWM));
  TEST_ASSERT_TRUE(runtime_config_is_safety(RCFG_FAN_PID_EN));
  TEST_ASSERT_FALSE(runtime_config_is_safety(RCFG_GPRS_ACT));
  TEST_ASSERT_FALSE(runtime_config_is_safety(RCFG_GPRS_PHOTO));
  TEST_ASSERT_FALSE(runtime_config_is_safety(RCFG_GPRS_STBY));
}

// --- regla estricta (consola USB y remoto) ---------------------------------

static void test_estricta_rechaza_los_cortes_fuera_de_34_40(void) {
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE,
                        run_strict(RCFG_AIR_TMAX, 45.0f, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE,
                        run_strict(RCFG_AIR_TMAX, 33.9f, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE,
                        run_strict(RCFG_SKIN_TMAX, 40.1f, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
}

static void test_estricta_acepta_los_cortes_dentro_de_rango(void) {
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_strict(RCFG_AIR_TMAX, 34.0f, &out));
  TEST_ASSERT_EQUAL_FLOAT(34.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_strict(RCFG_AIR_TMAX, 40.0f, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_strict(RCFG_SKIN_TMAX, 37.5f, &out));
  TEST_ASSERT_EQUAL_FLOAT(37.5f, out);
}

static void test_estricta_no_cambia_el_resto_de_reglas(void) {
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run_strict(RCFG_FAN_CTL_PWM, 0.0f, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_strict(RCFG_GPRS_ACT, 60.0f, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_FINITE,
                        run_strict(RCFG_AIR_TMAX, NAN_F, &out));
  // La regla con recorte (la del POST web) sigue recortando.
  TEST_ASSERT_EQUAL_INT(RCFG_OK_CLAMPED, run(RCFG_AIR_TMAX, 45.0f, &out));
}

// --- reglas del origen remoto (con el flag a 1) -----------------------------

// Unidad de ejemplo: fabrica 40/40/9.5, con tacometro, en standby.
static RuntimeConfigRemoteCtx ctx_ok(void) {
  RuntimeConfigRemoteCtx c;
  c.air_factory_max = 40.0f;
  c.skin_factory_max = 40.0f;
  c.heater_factory_max = 9.5f;
  c.fan_has_tach = true;
  c.heater_acting = false;
  c.air_setpoint_c = 0.0f;
  c.skin_setpoint_c = 0.0f;
  return c;
}

static RuntimeConfigStatus run_remote(RuntimeConfigId id, float in,
                                      const RuntimeConfigRemoteCtx &c,
                                      float *out) {
  *out = SENTINEL;
  return runtime_config_validate_remote(id, in, &c, out);
}

static void test_remoto_el_corte_solo_baja_o_se_mantiene(void) {
  RuntimeConfigRemoteCtx c = ctx_ok();
  c.air_factory_max = 38.0f;  // unidad de fabrica con el corte normativo
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_remote(RCFG_AIR_TMAX, 36.0f, c, &out));
  TEST_ASSERT_EQUAL_FLOAT(36.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_remote(RCFG_AIR_TMAX, 38.0f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_ABOVE_FACTORY,
                        run_remote(RCFG_AIR_TMAX, 38.5f, c, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_ABOVE_FACTORY,
                        run_remote(RCFG_AIR_TMAX, 40.0f, c, &out));
}

static void test_remoto_el_corte_de_piel_tiene_el_mismo_techo(void) {
  RuntimeConfigRemoteCtx c = ctx_ok();
  c.skin_factory_max = 37.0f;
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_remote(RCFG_SKIN_TMAX, 37.0f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_ABOVE_FACTORY,
                        run_remote(RCFG_SKIN_TMAX, 37.1f, c, &out));
}

static void test_remoto_los_cortes_fuera_de_34_40_se_rechazan(void) {
  const RuntimeConfigRemoteCtx c = ctx_ok();
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run_remote(RCFG_AIR_TMAX, 45.0f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run_remote(RCFG_AIR_TMAX, 30.0f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run_remote(RCFG_SKIN_TMAX, 41.0f, c, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
}

static void test_remoto_corte_por_debajo_de_la_consigna_activa_se_rechaza(void) {
  RuntimeConfigRemoteCtx c = ctx_ok();
  c.air_setpoint_c = 37.0f;  // control de aire en curso a 37 C
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_BELOW_SETPOINT,
                        run_remote(RCFG_AIR_TMAX, 36.0f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_BELOW_SETPOINT,
                        run_remote(RCFG_AIR_TMAX, 37.0f, c, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_remote(RCFG_AIR_TMAX, 37.5f, c, &out));
  // Sin control en curso (consigna 0) no hay nada que proteger.
  c.air_setpoint_c = 0.0f;
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_remote(RCFG_AIR_TMAX, 36.0f, c, &out));
}

static void test_remoto_corte_de_piel_por_debajo_de_su_consigna(void) {
  RuntimeConfigRemoteCtx c = ctx_ok();
  c.skin_setpoint_c = 36.5f;
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_BELOW_SETPOINT,
                        run_remote(RCFG_SKIN_TMAX, 36.0f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_remote(RCFG_SKIN_TMAX, 38.0f, c, &out));
}

static void test_remoto_heater_amps_solo_baja_o_se_mantiene(void) {
  const RuntimeConfigRemoteCtx c = ctx_ok();  // fabrica 9.5
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_remote(RCFG_HEATER_AMPS, 9.5f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_remote(RCFG_HEATER_AMPS, 5.0f, c, &out));
  TEST_ASSERT_EQUAL_FLOAT(5.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_ABOVE_FACTORY,
                        run_remote(RCFG_HEATER_AMPS, 9.6f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_ABOVE_FACTORY,
                        run_remote(RCFG_HEATER_AMPS, 12.0f, c, &out));
  // La regla base sigue mandando por debajo.
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run_remote(RCFG_HEATER_AMPS, 0.5f, c, &out));
}

static void test_remoto_ventilador_exige_tacometro(void) {
  RuntimeConfigRemoteCtx c = ctx_ok();
  c.fan_has_tach = false;
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NO_TACHOMETER,
                        run_remote(RCFG_FAN_CTL_PWM, 130.0f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NO_TACHOMETER,
                        run_remote(RCFG_FAN_SUPPLY_PWM, 200.0f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NO_TACHOMETER,
                        run_remote(RCFG_FAN_PID_EN, 1.0f, c, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
}

static void test_remoto_ventilador_se_rechaza_con_el_calefactor_actuando(void) {
  RuntimeConfigRemoteCtx c = ctx_ok();
  c.heater_acting = true;
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_HEATER_ACTING,
                        run_remote(RCFG_FAN_CTL_PWM, 130.0f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_HEATER_ACTING,
                        run_remote(RCFG_FAN_PID_EN, 0.0f, c, &out));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
}

static void test_remoto_pwm_del_ventilador_tiene_minimo(void) {
  const RuntimeConfigRemoteCtx c = ctx_ok();
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_OK,
                        run_remote(RCFG_FAN_CTL_PWM, RCFG_FAN_PWM_REMOTE_MIN, c, &out));
  TEST_ASSERT_EQUAL_FLOAT((float)RCFG_FAN_PWM_REMOTE_MIN, out);
  TEST_ASSERT_EQUAL_INT(
      RCFG_REJ_RANGE,
      run_remote(RCFG_FAN_CTL_PWM, RCFG_FAN_PWM_REMOTE_MIN - 1, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE,
                        run_remote(RCFG_FAN_SUPPLY_PWM, 10.0f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_remote(RCFG_FAN_SUPPLY_PWM, 255.0f, c, &out));
  // fan_pid_en no es un PWM: el minimo no le afecta.
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_remote(RCFG_FAN_PID_EN, 0.0f, c, &out));
}

static void test_remoto_conserva_la_regla_base_en_los_periodos(void) {
  const RuntimeConfigRemoteCtx c = ctx_ok();
  float out;
  TEST_ASSERT_EQUAL_INT(RCFG_OK, run_remote(RCFG_GPRS_STBY, 1800.0f, c, &out));
  TEST_ASSERT_EQUAL_FLOAT(1800.0f, out);
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_RANGE, run_remote(RCFG_GPRS_ACT, 5.0f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_INTEGER,
                        run_remote(RCFG_GPRS_PHOTO, 60.5f, c, &out));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_FINITE,
                        run_remote(RCFG_GPRS_ACT, INF_F, c, &out));
}

static void test_remoto_sin_contexto_o_salida_se_rechaza(void) {
  float out = SENTINEL;
  TEST_ASSERT_FALSE(runtime_config_status_accepted(
      runtime_config_validate_remote(RCFG_GPRS_ACT, 60.0f, nullptr, &out)));
  TEST_ASSERT_EQUAL_FLOAT(SENTINEL, out);
}

// --- aplazamiento y ritmo --------------------------------------------------

static void test_seguridad_con_terapia_activa_se_aplaza(void) {
  for (int i = 0; i < RCFG_COUNT; ++i) {
    const RuntimeConfigId id = (RuntimeConfigId)i;
    TEST_ASSERT_EQUAL_INT(runtime_config_is_safety(id),
                          runtime_config_should_defer(id, true));
    TEST_ASSERT_FALSE(runtime_config_should_defer(id, false));
  }
}

static void test_un_cambio_por_clave_cada_10_segundos(void) {
  TEST_ASSERT_TRUE(runtime_config_rate_ok(false, 0, 5, 10000));  // nunca antes
  TEST_ASSERT_FALSE(runtime_config_rate_ok(true, 1000, 10999, 10000));
  TEST_ASSERT_TRUE(runtime_config_rate_ok(true, 1000, 11000, 10000));
  TEST_ASSERT_TRUE(runtime_config_rate_ok(true, 1000, 60000, 10000));
}

static void test_el_ritmo_tolera_la_vuelta_del_contador(void) {
  // last justo antes del desbordamiento, now justo despues: pasaron 6 s.
  TEST_ASSERT_FALSE(runtime_config_rate_ok(true, 0xFFFFFFFFu - 2999u, 3000u, 10000));
  TEST_ASSERT_TRUE(runtime_config_rate_ok(true, 0xFFFFFFFFu - 2999u, 8000u, 10000));
}

// --- saneado de valores heredados de NVS -----------------------------------

static void test_saneado_de_periodos(void) {
  TEST_ASSERT_EQUAL_INT(60, runtime_config_sanitize_period(60, 999));
  TEST_ASSERT_EQUAL_INT(10, runtime_config_sanitize_period(10, 999));
  TEST_ASSERT_EQUAL_INT(86400, runtime_config_sanitize_period(86400, 999));
  TEST_ASSERT_EQUAL_INT(180, runtime_config_sanitize_period(0, 180));
  TEST_ASSERT_EQUAL_INT(180, runtime_config_sanitize_period(9, 180));
  TEST_ASSERT_EQUAL_INT(180, runtime_config_sanitize_period(-5, 180));
  TEST_ASSERT_EQUAL_INT(180, runtime_config_sanitize_period(86401, 180));
}

static void test_saneado_de_heater_amps(void) {
  TEST_ASSERT_EQUAL_FLOAT(9.5f, runtime_config_sanitize_heater_amps(9.5f, 7.5f));
  TEST_ASSERT_EQUAL_FLOAT(7.5f, runtime_config_sanitize_heater_amps(0.0f, 7.5f));
  TEST_ASSERT_EQUAL_FLOAT(7.5f, runtime_config_sanitize_heater_amps(0.5f, 7.5f));
  TEST_ASSERT_EQUAL_FLOAT(7.5f, runtime_config_sanitize_heater_amps(13.0f, 7.5f));
  TEST_ASSERT_EQUAL_FLOAT(7.5f, runtime_config_sanitize_heater_amps(NAN_F, 7.5f));
  TEST_ASSERT_EQUAL_FLOAT(7.5f, runtime_config_sanitize_heater_amps(INF_F, 7.5f));
}

// --- eco con origen y epoch, auditoria y cfg_rejected ----------------------

static void test_eco_lleva_origen_y_epoch_por_clave(void) {
  float v[RCFG_COUNT];
  fill_defaults(v);
  const char *src[RCFG_COUNT] = {};
  uint32_t ep[RCFG_COUNT] = {};
  src[RCFG_GPRS_STBY] = "tb-gprs";
  ep[RCFG_GPRS_STBY] = 1730000000u;
  src[RCFG_AIR_TMAX] = "web";  // sin epoch (reloj sin sincronizar)
  char buf[RCFG_ECHO_JSON_MAX];
  const size_t n = runtime_config_format_echo(
      v, RCFG_MASK(RCFG_GPRS_STBY) | RCFG_MASK(RCFG_AIR_TMAX) |
             RCFG_MASK(RCFG_GPRS_ACT),
      src, ep, buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  StaticJsonDocument<512> doc;
  TEST_ASSERT_EQUAL_INT(DeserializationError::Ok,
                        deserializeJson(doc, buf).code());
  TEST_ASSERT_EQUAL_STRING("tb-gprs@1730000000", doc["cfg_src_gprs_stby"]);
  TEST_ASSERT_EQUAL_STRING("web", doc["cfg_src_air_tmax"]);
  // Sin origen conocido no se inventa una clave.
  TEST_ASSERT_TRUE(doc["cfg_src_gprs_act"].isNull());
  TEST_ASSERT_EQUAL_INT(3 + 2, (int)doc.size());
}

static void test_eco_con_origen_de_las_nueve_cabe_en_el_buffer(void) {
  float v[RCFG_COUNT];
  v[RCFG_FAN_SUPPLY_PWM] = 255.0f;
  v[RCFG_FAN_CTL_PWM] = 255.0f;
  v[RCFG_FAN_PID_EN] = 0.0f;
  v[RCFG_HEATER_AMPS] = 12.0f;
  v[RCFG_AIR_TMAX] = 40.0f;
  v[RCFG_SKIN_TMAX] = 40.0f;
  v[RCFG_GPRS_ACT] = 86400.0f;
  v[RCFG_GPRS_PHOTO] = 86400.0f;
  v[RCFG_GPRS_STBY] = 86400.0f;
  const char *src[RCFG_COUNT];
  uint32_t ep[RCFG_COUNT];
  for (int i = 0; i < RCFG_COUNT; ++i) {
    src[i] = "tb-gprs";
    ep[i] = 4294967295u;
  }
  char buf[RCFG_ECHO_JSON_MAX];
  const size_t n =
      runtime_config_format_echo(v, RCFG_MASK_ALL, src, ep, buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_TRUE(n < sizeof(buf));
}

static void test_auditoria_cfg_change(void) {
  char buf[RCFG_AUDIT_JSON_MAX];
  const size_t n = runtime_config_format_audit(RCFG_AIR_TMAX, 38.0f, 40.0f,
                                               "tb-gprs", 1730000000u, buf,
                                               sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL_UINT(strlen(buf), n);
  StaticJsonDocument<256> doc;
  TEST_ASSERT_EQUAL_INT(DeserializationError::Ok,
                        deserializeJson(doc, buf).code());
  JsonObject e = doc["cfg_change"];
  TEST_ASSERT_FALSE(e.isNull());
  TEST_ASSERT_EQUAL_STRING("air_tmax", e["key"]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 38.0f, e["old"].as<float>());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, e["new"].as<float>());
  TEST_ASSERT_EQUAL_STRING("tb-gprs", e["origin"]);
  TEST_ASSERT_EQUAL_UINT32(1730000000u, e["epoch"].as<uint32_t>());
}

static void test_auditoria_de_un_entero_y_buffer_corto(void) {
  char buf[RCFG_AUDIT_JSON_MAX];
  TEST_ASSERT_TRUE(runtime_config_format_audit(RCFG_GPRS_STBY, 3600.0f, 1800.0f,
                                               "web", 0, buf, sizeof(buf)) > 0);
  StaticJsonDocument<256> doc;
  TEST_ASSERT_EQUAL_INT(DeserializationError::Ok,
                        deserializeJson(doc, buf).code());
  TEST_ASSERT_EQUAL_INT(1800, doc["cfg_change"]["new"].as<int>());
  TEST_ASSERT_EQUAL_UINT32(0, doc["cfg_change"]["epoch"].as<uint32_t>());
  char corto[20];
  TEST_ASSERT_EQUAL_UINT(
      0, runtime_config_format_audit(RCFG_GPRS_STBY, 1.0f, 2.0f, "web", 0, corto,
                                     sizeof(corto)));
  TEST_ASSERT_EQUAL_UINT(
      0, runtime_config_format_audit(RCFG_COUNT, 1.0f, 2.0f, "web", 0, buf,
                                     sizeof(buf)));
}

static void test_cfg_rejected(void) {
  char buf[96];
  const size_t n = runtime_config_format_rejected(
      "air_tmax", "not allowed remotely", buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  StaticJsonDocument<128> doc;
  TEST_ASSERT_EQUAL_INT(DeserializationError::Ok,
                        deserializeJson(doc, buf).code());
  TEST_ASSERT_EQUAL_STRING("air_tmax: not allowed remotely", doc["cfg_rejected"]);
  char corto[12];
  TEST_ASSERT_EQUAL_UINT(0, runtime_config_format_rejected("air_tmax", "x", corto,
                                                           sizeof(corto)));
}

static void test_cfg_rejected_no_rompe_el_json_con_comillas(void) {
  // Los textos son del firmware, pero el formateador no debe romper el JSON
  // si algun dia alguien mete una comilla o una barra.
  char buf[96];
  const size_t n =
      runtime_config_format_rejected("a\"b", "c\\d", buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  StaticJsonDocument<128> doc;
  TEST_ASSERT_EQUAL_INT(DeserializationError::Ok,
                        deserializeJson(doc, buf).code());
}

// --- linea de la consola USB ---------------------------------------------

static void test_consola_linea_valida(void) {
  char param[32], raw[32];
  TEST_ASSERT_TRUE(runtime_config_parse_console_line("/config,AIR_TMAX,38", param,
                                                     sizeof(param), raw,
                                                     sizeof(raw)));
  TEST_ASSERT_EQUAL_STRING("AIR_TMAX", param);
  TEST_ASSERT_EQUAL_STRING("38", raw);
  TEST_ASSERT_TRUE(runtime_config_parse_console_line(
      "/config,GPRS_STBY,1800\r\n", param, sizeof(param), raw, sizeof(raw)));
  TEST_ASSERT_EQUAL_STRING("GPRS_STBY", param);
  TEST_ASSERT_EQUAL_STRING("1800", raw);
  TEST_ASSERT_TRUE(runtime_config_parse_console_line(
      "/config,AIR_TMAX,38.5  ", param, sizeof(param), raw, sizeof(raw)));
  TEST_ASSERT_EQUAL_STRING("38.5", raw);
}

static void test_consola_rechaza_texto_despues_del_valor(void) {
  char param[32], raw[32];
  const char *malas[] = {"/config,AIR_TMAX,38 99", "/config,AIR_TMAX,38,99",
                         "/config,AIR_TMAX,38abc", "/config,AIR_TMAX,38 x\n",
                         "/config,AIR_TMAX,38;"};
  for (unsigned i = 0; i < sizeof(malas) / sizeof(malas[0]); ++i) {
    TEST_ASSERT_FALSE_MESSAGE(
        runtime_config_parse_console_line(malas[i], param, sizeof(param), raw,
                                          sizeof(raw)),
        malas[i]);
  }
}

static void test_consola_rechaza_lineas_mal_formadas(void) {
  char param[32], raw[32];
  const char *malas[] = {"/config", "/config,", "/config,AIR_TMAX",
                         "/config,AIR_TMAX,", "/config,,38", "config,AIR_TMAX,38",
                         "", "/configX,AIR_TMAX,38"};
  for (unsigned i = 0; i < sizeof(malas) / sizeof(malas[0]); ++i) {
    TEST_ASSERT_FALSE_MESSAGE(
        runtime_config_parse_console_line(malas[i], param, sizeof(param), raw,
                                          sizeof(raw)),
        malas[i]);
  }
  TEST_ASSERT_FALSE(runtime_config_parse_console_line(nullptr, param,
                                                      sizeof(param), raw,
                                                      sizeof(raw)));
  // Nombre o valor que no caben: se rechaza, no se trunca.
  TEST_ASSERT_FALSE(runtime_config_parse_console_line(
      "/config,AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA,1", param, sizeof(param), raw,
      sizeof(raw)));
  TEST_ASSERT_FALSE(runtime_config_parse_console_line(
      "/config,AIR_TMAX,1234567890123456789012345678901234567890", param,
      sizeof(param), raw, sizeof(raw)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_los_nueve_nombres_resuelven_a_su_id);
  RUN_TEST(test_lo_que_no_esta_en_la_lista_blanca_no_resuelve);
  RUN_TEST(test_la_web_y_thingsboard_son_sensibles_a_mayusculas);
  RUN_TEST(test_la_consola_acepta_solo_las_mayusculas_exactas);
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
  RUN_TEST(test_json_booleano_solo_si_se_permite);
  RUN_TEST(test_json_rechaza_el_resto_de_tipos);
  RUN_TEST(test_eco_solo_lleva_las_claves_pendientes);
  RUN_TEST(test_eco_de_fan_pid_en_es_booleano);
  RUN_TEST(test_eco_completo_a_valores_maximos_cabe);
  RUN_TEST(test_eco_con_mascara_vacia_o_buffer_corto_devuelve_cero);
  RUN_TEST(test_eco_ignora_bits_fuera_de_los_nueve);
  RUN_TEST(test_con_el_flag_a_0_solo_los_periodos_son_remotos);
  RUN_TEST(test_con_el_flag_a_1_son_remotos_los_nueve);
  RUN_TEST(test_seguridad_son_los_seis_que_no_son_periodos);
  RUN_TEST(test_estricta_rechaza_los_cortes_fuera_de_34_40);
  RUN_TEST(test_estricta_acepta_los_cortes_dentro_de_rango);
  RUN_TEST(test_estricta_no_cambia_el_resto_de_reglas);
  RUN_TEST(test_remoto_el_corte_solo_baja_o_se_mantiene);
  RUN_TEST(test_remoto_el_corte_de_piel_tiene_el_mismo_techo);
  RUN_TEST(test_remoto_los_cortes_fuera_de_34_40_se_rechazan);
  RUN_TEST(test_remoto_corte_por_debajo_de_la_consigna_activa_se_rechaza);
  RUN_TEST(test_remoto_corte_de_piel_por_debajo_de_su_consigna);
  RUN_TEST(test_remoto_heater_amps_solo_baja_o_se_mantiene);
  RUN_TEST(test_remoto_ventilador_exige_tacometro);
  RUN_TEST(test_remoto_ventilador_se_rechaza_con_el_calefactor_actuando);
  RUN_TEST(test_remoto_pwm_del_ventilador_tiene_minimo);
  RUN_TEST(test_remoto_conserva_la_regla_base_en_los_periodos);
  RUN_TEST(test_remoto_sin_contexto_o_salida_se_rechaza);
  RUN_TEST(test_seguridad_con_terapia_activa_se_aplaza);
  RUN_TEST(test_un_cambio_por_clave_cada_10_segundos);
  RUN_TEST(test_el_ritmo_tolera_la_vuelta_del_contador);
  RUN_TEST(test_saneado_de_periodos);
  RUN_TEST(test_saneado_de_heater_amps);
  RUN_TEST(test_eco_lleva_origen_y_epoch_por_clave);
  RUN_TEST(test_eco_con_origen_de_las_nueve_cabe_en_el_buffer);
  RUN_TEST(test_auditoria_cfg_change);
  RUN_TEST(test_auditoria_de_un_entero_y_buffer_corto);
  RUN_TEST(test_cfg_rejected);
  RUN_TEST(test_cfg_rejected_no_rompe_el_json_con_comillas);
  RUN_TEST(test_consola_linea_valida);
  RUN_TEST(test_consola_rechaza_texto_despues_del_valor);
  RUN_TEST(test_consola_rechaza_lineas_mal_formadas);
  RUN_TEST(test_consola_linea_valida);
  RUN_TEST(test_consola_rechaza_texto_despues_del_valor);
  RUN_TEST(test_consola_rechaza_lineas_mal_formadas);
  return UNITY_END();
}

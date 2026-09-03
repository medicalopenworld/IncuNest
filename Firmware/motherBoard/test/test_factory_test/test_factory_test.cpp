#include <unity.h>

#include <cstring>

#include "factory_test.h"
#include "modules/factory_test/ftest_summary.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Tabla: rangos, flags y claves (factory-test-protocol, "Consulta de la
// tabla" / "Identificador fuera de tabla").
// ---------------------------------------------------------------------------

void test_table_ranges_and_optional_flags(void) {
  for (unsigned id = 0; id < FTEST_MB_COUNT; ++id) {
    TEST_ASSERT_TRUE_MESSAGE(ftest_id_is_mb(id), "id de motherBoard");
    TEST_ASSERT_FALSE_MESSAGE(ftest_id_is_hmi(id), "no puede ser tambien HMI");
  }
  for (unsigned id = FTEST_HMI_BASE; id < FTEST_HMI_END; ++id) {
    TEST_ASSERT_FALSE_MESSAGE(ftest_id_is_mb(id), "id de display, no de mb");
    TEST_ASSERT_TRUE_MESSAGE(ftest_id_is_hmi(id), "id de display");
  }

  TEST_ASSERT_TRUE(ftest_id_is_optional(FTEST_MB_GSM_NET));
  TEST_ASSERT_TRUE(ftest_id_is_optional(FTEST_MB_WIFI));
  TEST_ASSERT_TRUE(ftest_id_is_optional(FTEST_MB_TB_PROVISION));
  TEST_ASSERT_TRUE(ftest_id_is_optional(FTEST_MB_TIME));
  TEST_ASSERT_TRUE(ftest_id_is_optional(FTEST_MB_AFE_PROBE));

  TEST_ASSERT_FALSE(ftest_id_is_optional(FTEST_MB_ACTUATORS));
  TEST_ASSERT_FALSE(ftest_id_is_optional(FTEST_MB_SENSOR_SRC));
  TEST_ASSERT_FALSE(ftest_id_is_optional(FTEST_MB_SB_LINK));
}

void test_table_keys_are_short_and_present(void) {
  for (unsigned id = 0; id < FTEST_MB_COUNT; ++id) {
    const char *key = ftest_id_key(id);
    TEST_ASSERT_NOT_NULL(key);
    TEST_ASSERT_TRUE(strlen(key) > 0);
    TEST_ASSERT_TRUE(strlen(key) <= 12);
    TEST_ASSERT_NOT_EQUAL(0, strcmp(key, "?"));
  }
  for (unsigned id = FTEST_HMI_BASE; id < FTEST_HMI_END; ++id) {
    const char *key = ftest_id_key(id);
    TEST_ASSERT_NOT_NULL(key);
    TEST_ASSERT_TRUE(strlen(key) > 0);
    TEST_ASSERT_TRUE(strlen(key) <= 12);
    TEST_ASSERT_NOT_EQUAL(0, strcmp(key, "?"));
  }
}

void test_id_out_of_table(void) {
  TEST_ASSERT_FALSE(ftest_id_is_mb(63));
  TEST_ASSERT_FALSE(ftest_id_is_mb(200));
  TEST_ASSERT_FALSE(ftest_id_is_hmi(63));
  TEST_ASSERT_FALSE(ftest_id_is_hmi(200));
  TEST_ASSERT_EQUAL_STRING("?", ftest_id_key(63));
  TEST_ASSERT_EQUAL_STRING("?", ftest_id_key(200));
}

// ---------------------------------------------------------------------------
// Codificar/parsear CTRL,FTEST (factory-test-protocol, "Linea de resultado").
// ---------------------------------------------------------------------------

void test_format_result_basic(void) {
  char buf[FTEST_TX_LINE_MAX];
  const int written = ftest_format_result(buf, sizeof(buf), FTEST_MB_SB_ENV,
                                           FTEST_PASS, "36.1/36.2/36.0");
  TEST_ASSERT_EQUAL_STRING("CTRL,FTEST,10,1,36.1/36.2/36.0\n", buf);
  TEST_ASSERT_EQUAL_INT((int)strlen(buf), written);
}

void test_format_result_sanitizes_and_truncates_detail(void) {
  // 60 caracteres con comas de sobra.
  char longDetail[61];
  for (int i = 0; i < 60; ++i) {
    longDetail[i] = 'a';
  }
  longDetail[10] = ',';
  longDetail[30] = ',';
  longDetail[60] = '\0';

  char buf[FTEST_TX_LINE_MAX];
  const int written = ftest_format_result(buf, sizeof(buf), FTEST_MB_BUZZER,
                                           FTEST_FAIL, longDetail);
  TEST_ASSERT_TRUE(written > 0);
  TEST_ASSERT_TRUE(strlen(buf) <= FTEST_TX_LINE_MAX);
  TEST_ASSERT_EQUAL_INT(0, strncmp(buf, "CTRL,FTEST,17,2,", 16));

  // El campo detail no debe contener comas (las originales se sanearon).
  const char *detailStart = strrchr(buf, ',') + 1;
  TEST_ASSERT_NULL(strchr(detailStart, ','));
  // 40 caracteres de detail + '\n'.
  TEST_ASSERT_EQUAL_UINT32(FTEST_DETAIL_MAX + 1u,
                            (uint32_t)strlen(detailStart));
  TEST_ASSERT_EQUAL_CHAR(';', detailStart[10]);
  TEST_ASSERT_EQUAL_CHAR(';', detailStart[30]);
}

void test_parse_result_valid_with_empty_detail(void) {
  FtestResult out;
  memset(&out, 0, sizeof(out));
  TEST_ASSERT_TRUE(ftest_parse_result("11,4,\n", &out));
  TEST_ASSERT_EQUAL_UINT8(11, out.id);
  TEST_ASSERT_EQUAL(FTEST_WAIT, out.status);
  TEST_ASSERT_EQUAL_STRING("", out.detail);
}

void test_parse_result_discards_malformed_lines(void) {
  FtestResult sentinel;
  sentinel.id = 123;
  sentinel.status = (FtestStatus)77;
  strcpy(sentinel.detail, "untouched");

  FtestResult out = sentinel;
  TEST_ASSERT_FALSE(ftest_parse_result("11", &out)); // faltan campos
  TEST_ASSERT_EQUAL_UINT8(sentinel.id, out.id);
  TEST_ASSERT_EQUAL_STRING(sentinel.detail, out.detail);

  out = sentinel;
  TEST_ASSERT_FALSE(ftest_parse_result("x,1,", &out)); // id no numerico
  TEST_ASSERT_EQUAL_UINT8(sentinel.id, out.id);

  out = sentinel;
  TEST_ASSERT_FALSE(ftest_parse_result("11,9,", &out)); // status fuera de rango
  TEST_ASSERT_EQUAL_UINT8(sentinel.id, out.id);

  out = sentinel;
  TEST_ASSERT_FALSE(ftest_parse_result("99,1,", &out)); // id fuera de tabla
  TEST_ASSERT_EQUAL_UINT8(sentinel.id, out.id);
}

// Hallazgo de seguridad: un campo numerico de mas de 3 digitos (todo el
// protocolo cabe en 0..255) se rechaza ANTES de mirar el valor, en vez de
// confiar en el desbordamiento/ERANGE de strtoul() para un numero como
// "4294967296" (2^32, da la vuelta a 0 en unsigned long de 32 bits).
void test_parse_rejects_oversized_numeric_fields(void) {
  FtestResult out;
  memset(&out, 0, sizeof(out));
  TEST_ASSERT_FALSE(ftest_parse_result("4294967296,1,", &out));

  FtestHmiCmd cmd;
  TEST_ASSERT_FALSE(ftest_parse_hmi_cmd("RUN,4294967296", &cmd));
}

// ---------------------------------------------------------------------------
// Cierre y rechazo de la bateria.
// ---------------------------------------------------------------------------

void test_format_and_parse_done_roundtrip(void) {
  char buf[FTEST_TX_LINE_MAX];
  const int written = ftest_format_done(buf, sizeof(buf), 25, 2, 3);
  TEST_ASSERT_EQUAL_STRING("CTRL,FTEST_DONE,25,2,3\n", buf);
  TEST_ASSERT_EQUAL_INT((int)strlen(buf), written);

  unsigned pass = 0, fail = 0, skip = 0;
  TEST_ASSERT_TRUE(ftest_parse_done("25,2,3\n", &pass, &fail, &skip));
  TEST_ASSERT_EQUAL_UINT(25, pass);
  TEST_ASSERT_EQUAL_UINT(2, fail);
  TEST_ASSERT_EQUAL_UINT(3, skip);
}

void test_parse_done_malformed(void) {
  unsigned pass = 111, fail = 111, skip = 111;
  TEST_ASSERT_FALSE(ftest_parse_done("25,2", &pass, &fail, &skip));
  TEST_ASSERT_EQUAL_UINT(111, pass);
  TEST_ASSERT_EQUAL_UINT(111, fail);
  TEST_ASSERT_EQUAL_UINT(111, skip);

  TEST_ASSERT_FALSE(ftest_parse_done("a,b,c", &pass, &fail, &skip));
  TEST_ASSERT_EQUAL_UINT(111, pass);
}

void test_format_and_parse_reject_roundtrip(void) {
  char buf[FTEST_TX_LINE_MAX];
  const int written =
      ftest_format_reject(buf, sizeof(buf), FTEST_REJECT_CONTROL_ACTIVE);
  TEST_ASSERT_EQUAL_STRING("CTRL,FTEST_REJECT,1\n", buf);
  TEST_ASSERT_EQUAL_INT((int)strlen(buf), written);

  FtestReject reason;
  TEST_ASSERT_TRUE(ftest_parse_reject("1\n", &reason));
  TEST_ASSERT_EQUAL(FTEST_REJECT_CONTROL_ACTIVE, reason);

  TEST_ASSERT_FALSE(ftest_parse_reject("9\n", &reason));
}

// ---------------------------------------------------------------------------
// Comandos HMI,FTEST.
// ---------------------------------------------------------------------------

void test_parse_hmi_cmd_valid_commands(void) {
  FtestHmiCmd cmd;

  TEST_ASSERT_TRUE(ftest_parse_hmi_cmd("START", &cmd));
  TEST_ASSERT_EQUAL(FTEST_CMD_START, cmd.type);

  TEST_ASSERT_TRUE(ftest_parse_hmi_cmd("RUN,14", &cmd));
  TEST_ASSERT_EQUAL(FTEST_CMD_RUN, cmd.type);
  TEST_ASSERT_EQUAL_UINT8(14, cmd.id);

  TEST_ASSERT_TRUE(ftest_parse_hmi_cmd("ABORT", &cmd));
  TEST_ASSERT_EQUAL(FTEST_CMD_ABORT, cmd.type);

  TEST_ASSERT_TRUE(ftest_parse_hmi_cmd("CONFIRM,17,1", &cmd));
  TEST_ASSERT_EQUAL(FTEST_CMD_CONFIRM, cmd.type);
  TEST_ASSERT_EQUAL_UINT8(17, cmd.id);
  TEST_ASSERT_TRUE(cmd.ok);
}

void test_parse_hmi_cmd_invalid_commands(void) {
  FtestHmiCmd cmd;
  TEST_ASSERT_FALSE(ftest_parse_hmi_cmd("RUN", &cmd));         // sin id
  TEST_ASSERT_FALSE(ftest_parse_hmi_cmd("RUN,64", &cmd));      // id de display
  TEST_ASSERT_FALSE(ftest_parse_hmi_cmd("CONFIRM,17", &cmd));  // falta ok
  TEST_ASSERT_FALSE(ftest_parse_hmi_cmd("CONFIRM,17,2", &cmd)); // ok invalido
  TEST_ASSERT_FALSE(ftest_parse_hmi_cmd("FOO", &cmd));         // desconocido
}

// ---------------------------------------------------------------------------
// ftest_summary: acumulacion (mb-factory-test, "Batería completa en orden
// fijo, un resultado por test").
// ---------------------------------------------------------------------------

void test_summary_accumulates_pass_fail_skip(void) {
  FtestSummary s;
  ftest_summary_init(&s);

  ftest_summary_note(&s, 0, FTEST_PASS);
  ftest_summary_note(&s, 1, FTEST_PASS);
  ftest_summary_note(&s, 2, FTEST_FAIL);
  ftest_summary_note(&s, 3, FTEST_SKIP);
  ftest_summary_note(&s, 4, FTEST_PASS);

  TEST_ASSERT_EQUAL_UINT8(3, s.pass);
  TEST_ASSERT_EQUAL_UINT8(1, s.fail);
  TEST_ASSERT_EQUAL_UINT8(1, s.skip);
  TEST_ASSERT_EQUAL_UINT32(0b10011u, s.pass_mask);
  TEST_ASSERT_EQUAL_UINT32(0b00100u, s.fail_mask);
  TEST_ASSERT_EQUAL_UINT32(0b11111u, s.run_mask);
}

void test_summary_running_wait_confirm_do_not_count(void) {
  FtestSummary s;
  ftest_summary_init(&s);

  ftest_summary_note(&s, 2, FTEST_RUNNING);
  ftest_summary_note(&s, 2, FTEST_WAIT);
  ftest_summary_note(&s, 2, FTEST_WAIT);
  ftest_summary_note(&s, 2, FTEST_CONFIRM);
  ftest_summary_note(&s, 2, FTEST_PASS);

  TEST_ASSERT_EQUAL_UINT8(1, s.pass);
  TEST_ASSERT_EQUAL_UINT8(0, s.fail);
  TEST_ASSERT_EQUAL_UINT8(0, s.skip);
  TEST_ASSERT_EQUAL_UINT32((1u << 2), s.run_mask);
}

void test_summary_ignores_non_mb_ids(void) {
  FtestSummary s;
  ftest_summary_init(&s);

  ftest_summary_note(&s, FTEST_HMI_TOUCH, FTEST_PASS);
  ftest_summary_note(&s, 200, FTEST_FAIL);

  TEST_ASSERT_EQUAL_UINT8(0, s.pass);
  TEST_ASSERT_EQUAL_UINT8(0, s.fail);
  TEST_ASSERT_EQUAL_UINT32(0u, s.run_mask);
}

void test_summary_second_final_notification_is_ignored(void) {
  FtestSummary s;
  ftest_summary_init(&s);

  ftest_summary_note(&s, 5, FTEST_FAIL);
  ftest_summary_note(&s, 5, FTEST_PASS); // ya contabilizado: no cambia nada

  TEST_ASSERT_EQUAL_UINT8(0, s.pass);
  TEST_ASSERT_EQUAL_UINT8(1, s.fail);
  TEST_ASSERT_EQUAL_UINT32((1u << 5), s.fail_mask);
  TEST_ASSERT_EQUAL_UINT32(0u, s.pass_mask);
}

// ---------------------------------------------------------------------------
// ftest_summary_merge_single: reintento (mb-factory-test, "Persistencia del
// resultado" / "Reintento de un test fallido").
// ---------------------------------------------------------------------------

void test_summary_merge_single_moves_bit_from_fail_to_pass(void) {
  uint32_t passMask = (1u << 2);              // otro test ya en PASA
  uint32_t failMask = (1u << 13);             // el 13 fallo en la bateria
  uint32_t runMask = (1u << 2) | (1u << 13);

  ftest_summary_merge_single(&passMask, &failMask, &runMask, 13, FTEST_PASS);

  TEST_ASSERT_EQUAL_UINT32((1u << 2) | (1u << 13), passMask);
  TEST_ASSERT_EQUAL_UINT32(0u, failMask);
  TEST_ASSERT_EQUAL_UINT32((1u << 2) | (1u << 13), runMask);
}

void test_summary_merge_single_skip_only_marks_run(void) {
  uint32_t passMask = (1u << 3);
  uint32_t failMask = 0u;
  uint32_t runMask = (1u << 3);

  ftest_summary_merge_single(&passMask, &failMask, &runMask, 3, FTEST_SKIP);

  TEST_ASSERT_EQUAL_UINT32(0u, passMask);
  TEST_ASSERT_EQUAL_UINT32(0u, failMask);
  TEST_ASSERT_EQUAL_UINT32((1u << 3), runMask);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_table_ranges_and_optional_flags);
  RUN_TEST(test_table_keys_are_short_and_present);
  RUN_TEST(test_id_out_of_table);

  RUN_TEST(test_format_result_basic);
  RUN_TEST(test_format_result_sanitizes_and_truncates_detail);
  RUN_TEST(test_parse_result_valid_with_empty_detail);
  RUN_TEST(test_parse_result_discards_malformed_lines);
  RUN_TEST(test_parse_rejects_oversized_numeric_fields);

  RUN_TEST(test_format_and_parse_done_roundtrip);
  RUN_TEST(test_parse_done_malformed);
  RUN_TEST(test_format_and_parse_reject_roundtrip);

  RUN_TEST(test_parse_hmi_cmd_valid_commands);
  RUN_TEST(test_parse_hmi_cmd_invalid_commands);

  RUN_TEST(test_summary_accumulates_pass_fail_skip);
  RUN_TEST(test_summary_running_wait_confirm_do_not_count);
  RUN_TEST(test_summary_ignores_non_mb_ids);
  RUN_TEST(test_summary_second_final_notification_is_ignored);
  RUN_TEST(test_summary_merge_single_moves_bit_from_fail_to_pass);
  RUN_TEST(test_summary_merge_single_skip_only_marks_run);

  return UNITY_END();
}

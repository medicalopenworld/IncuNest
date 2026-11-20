#include <string.h>
#include <unity.h>

#include "service_hours_proto.h"

void setUp(void) {}
void tearDown(void) {}

static ServiceReport sample(void) {
  ServiceReport r;
  memset(&r, 0, sizeof(r));
  r.n = 4;
  const uint32_t hours[4] = {12, 30, 2001, 0};
  const uint32_t thr[4] = {5000, 10000, 2000, 10000};
  for (uint8_t i = 0; i < 4; i++) {
    r.entry[i].id = i;
    r.entry[i].hours = hours[i];
    r.entry[i].thresholdH = thr[i];
  }
  return r;
}

// ---------------- CTRL,SERVICE ----------------

void test_build_report_exact_line(void) {
  const ServiceReport r = sample();
  char buf[96];
  const int n = service_proto_build_report(buf, sizeof(buf), &r);
  TEST_ASSERT_EQUAL_STRING(
      "CTRL,SERVICE,4,0,12,5000,1,30,10000,2,2001,2000,3,0,10000\n", buf);
  TEST_ASSERT_EQUAL_INT((int)strlen(buf), n);
}

void test_build_report_empty(void) {
  ServiceReport r;
  memset(&r, 0, sizeof(r));
  char buf[32];
  TEST_ASSERT_GREATER_THAN(0, service_proto_build_report(buf, sizeof(buf), &r));
  TEST_ASSERT_EQUAL_STRING("CTRL,SERVICE,0\n", buf);
}

void test_build_report_too_small_returns_zero(void) {
  const ServiceReport r = sample();
  char buf[20];
  TEST_ASSERT_EQUAL_INT(0, service_proto_build_report(buf, sizeof(buf), &r));
}

void test_build_report_rejects_bad_count(void) {
  ServiceReport r = sample();
  r.n = SVC_COUNT + 1;
  char buf[200];
  TEST_ASSERT_EQUAL_INT(0, service_proto_build_report(buf, sizeof(buf), &r));
}

void test_report_roundtrip(void) {
  const ServiceReport r = sample();
  char buf[96];
  TEST_ASSERT_GREATER_THAN(0, service_proto_build_report(buf, sizeof(buf), &r));
  buf[strlen(buf) - 1] = '\0';  // la capa serie quita el '\n'
  ServiceReport back;
  TEST_ASSERT_TRUE(service_proto_parse_report(buf, &back));
  TEST_ASSERT_EQUAL_UINT8(4, back.n);
  for (uint8_t i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL_UINT8(r.entry[i].id, back.entry[i].id);
    TEST_ASSERT_EQUAL_UINT32(r.entry[i].hours, back.entry[i].hours);
    TEST_ASSERT_EQUAL_UINT32(r.entry[i].thresholdH, back.entry[i].thresholdH);
  }
}

void test_parse_report_empty_list_is_valid(void) {
  ServiceReport r;
  TEST_ASSERT_TRUE(service_proto_parse_report("CTRL,SERVICE,0", &r));
  TEST_ASSERT_EQUAL_UINT8(0, r.n);
}

void test_parse_report_rejects_malformed(void) {
  ServiceReport r;
  // n dice 4 pero solo hay tres entradas
  TEST_ASSERT_FALSE(service_proto_parse_report(
      "CTRL,SERVICE,4,0,12,5000,1,30,10000,2,2001,2000", &r));
  // entrada a medias
  TEST_ASSERT_FALSE(service_proto_parse_report("CTRL,SERVICE,1,0,12", &r));
  // campos de mas
  TEST_ASSERT_FALSE(
      service_proto_parse_report("CTRL,SERVICE,1,0,12,5000,7", &r));
  // no numerico, vacio, negativo
  TEST_ASSERT_FALSE(service_proto_parse_report("CTRL,SERVICE,1,0,x,5000", &r));
  TEST_ASSERT_FALSE(service_proto_parse_report("CTRL,SERVICE,1,0,,5000", &r));
  TEST_ASSERT_FALSE(service_proto_parse_report("CTRL,SERVICE,1,0,-4,5000", &r));
  // id desconocido y repetido
  TEST_ASSERT_FALSE(service_proto_parse_report("CTRL,SERVICE,1,9,12,5000", &r));
  TEST_ASSERT_FALSE(
      service_proto_parse_report("CTRL,SERVICE,2,1,1,5,1,2,5", &r));
  // n fuera de rango
  TEST_ASSERT_FALSE(service_proto_parse_report("CTRL,SERVICE,5", &r));
  // otra linea / nula
  TEST_ASSERT_FALSE(service_proto_parse_report("CTRL,STATE,1", &r));
  TEST_ASSERT_FALSE(service_proto_parse_report("CTRL,SERVICE", &r));
  TEST_ASSERT_FALSE(service_proto_parse_report(nullptr, &r));
}

// Un parseo fallido no debe dejar datos a medias que alguien pueda leer.
void test_parse_report_failure_leaves_empty_report(void) {
  ServiceReport r;
  r.n = 3;
  TEST_ASSERT_FALSE(service_proto_parse_report("CTRL,SERVICE,2,0,1,5", &r));
  TEST_ASSERT_EQUAL_UINT8(0, r.n);
}

// ---------------- HMI,SERVICE_DONE / HMI,SERVICE_REQ ----------------

void test_build_done(void) {
  char buf[32];
  TEST_ASSERT_GREATER_THAN(0, service_proto_build_done(buf, sizeof(buf), 2));
  TEST_ASSERT_EQUAL_STRING("HMI,SERVICE_DONE,2\n", buf);
}

void test_build_done_rejects_bad_id_and_small_buffer(void) {
  char buf[32];
  TEST_ASSERT_EQUAL_INT(0, service_proto_build_done(buf, sizeof(buf), SVC_COUNT));
  char tiny[8];
  TEST_ASSERT_EQUAL_INT(0, service_proto_build_done(tiny, sizeof(tiny), 1));
}

void test_parse_done_accepts_each_valid_id(void) {
  uint8_t id = 99;
  TEST_ASSERT_TRUE(service_proto_parse_done("HMI,SERVICE_DONE,0", &id));
  TEST_ASSERT_EQUAL_UINT8(0, id);
  TEST_ASSERT_TRUE(service_proto_parse_done("HMI,SERVICE_DONE,3", &id));
  TEST_ASSERT_EQUAL_UINT8(3, id);
}

void test_parse_done_rejects_garbage(void) {
  uint8_t id = 99;
  TEST_ASSERT_FALSE(service_proto_parse_done("HMI,SERVICE_DONE,9", &id));
  TEST_ASSERT_FALSE(service_proto_parse_done("HMI,SERVICE_DONE,", &id));
  TEST_ASSERT_FALSE(service_proto_parse_done("HMI,SERVICE_DONE", &id));
  TEST_ASSERT_FALSE(service_proto_parse_done("HMI,SERVICE_DONE,1,2", &id));
  TEST_ASSERT_FALSE(service_proto_parse_done("HMI,SERVICE_DONE,-1", &id));
  TEST_ASSERT_FALSE(service_proto_parse_done("HMI,SERVICE_DONE,1x", &id));
  TEST_ASSERT_FALSE(service_proto_parse_done("HMI,SERVICE_REQ", &id));
  TEST_ASSERT_FALSE(service_proto_parse_done(nullptr, &id));
  TEST_ASSERT_EQUAL_UINT8(99, id);
}

void test_is_req_is_exact(void) {
  TEST_ASSERT_TRUE(service_proto_is_req("HMI,SERVICE_REQ"));
  TEST_ASSERT_FALSE(service_proto_is_req("HMI,SERVICE_REQ,1"));
  TEST_ASSERT_FALSE(service_proto_is_req("HMI,SERVICE_REQX"));
  TEST_ASSERT_FALSE(service_proto_is_req("HMI,SERVICE_DONE,1"));
  TEST_ASSERT_FALSE(service_proto_is_req(nullptr));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_build_report_exact_line);
  RUN_TEST(test_build_report_empty);
  RUN_TEST(test_build_report_too_small_returns_zero);
  RUN_TEST(test_build_report_rejects_bad_count);
  RUN_TEST(test_report_roundtrip);
  RUN_TEST(test_parse_report_empty_list_is_valid);
  RUN_TEST(test_parse_report_rejects_malformed);
  RUN_TEST(test_parse_report_failure_leaves_empty_report);
  RUN_TEST(test_build_done);
  RUN_TEST(test_build_done_rejects_bad_id_and_small_buffer);
  RUN_TEST(test_parse_done_accepts_each_valid_id);
  RUN_TEST(test_parse_done_rejects_garbage);
  RUN_TEST(test_is_req_is_exact);
  return UNITY_END();
}

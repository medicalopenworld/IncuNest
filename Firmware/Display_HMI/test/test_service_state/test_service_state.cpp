#include <string.h>
#include <unity.h>

#include "modules/service/service_state.h"

void setUp(void) { ServiceState_Reset(); }
void tearDown(void) {}

static ServiceReport report(uint32_t h0, uint32_t h1, uint32_t h2, uint32_t h3) {
  ServiceReport r;
  memset(&r, 0, sizeof(r));
  r.n = SVC_COUNT;
  const uint32_t hours[SVC_COUNT] = {h0, h1, h2, h3};
  const uint32_t thr[SVC_COUNT] = {5000, 10000, 2000, 10000};
  for (uint8_t i = 0; i < SVC_COUNT; i++) {
    r.entry[i].id = i;
    r.entry[i].hours = hours[i];
    r.entry[i].thresholdH = thr[i];
  }
  return r;
}

static const uint32_t DAY = 86400u;

// ---------------- Sin datos ----------------

void test_no_data_before_first_report(void) {
  TEST_ASSERT_FALSE(ServiceState_HasData());
  TEST_ASSERT_FALSE(ServiceState_AnyDue());
  TEST_ASSERT_FALSE(ServiceState_ShouldWarn(1000));
  uint32_t h, t;
  TEST_ASSERT_FALSE(ServiceState_Get(SVC_FAN, &h, &t));
}

// ---------------- Aplicar y consultar ----------------

void test_apply_stores_hours_and_threshold(void) {
  const ServiceReport r = report(10, 20, 30, 40);
  ServiceState_Apply(&r);
  TEST_ASSERT_TRUE(ServiceState_HasData());
  uint32_t h = 0, t = 0;
  TEST_ASSERT_TRUE(ServiceState_Get(SVC_HUMIDIFIER, &h, &t));
  TEST_ASSERT_EQUAL_UINT32(30, h);
  TEST_ASSERT_EQUAL_UINT32(2000, t);
  TEST_ASSERT_FALSE(ServiceState_AnyDue());
}

// Un informe parcial (n < 4) solo actualiza lo que trae; el resto se
// queda como estaba.
void test_partial_report_keeps_other_actuators(void) {
  const ServiceReport full = report(10, 20, 30, 40);
  ServiceState_Apply(&full);
  ServiceReport part;
  memset(&part, 0, sizeof(part));
  part.n = 1;
  part.entry[0].id = SVC_FAN;
  part.entry[0].hours = 77;
  part.entry[0].thresholdH = 10000;
  ServiceState_Apply(&part);
  uint32_t h = 0, t = 0;
  TEST_ASSERT_TRUE(ServiceState_Get(SVC_FAN, &h, &t));
  TEST_ASSERT_EQUAL_UINT32(77, h);
  TEST_ASSERT_TRUE(ServiceState_Get(SVC_HEATER, &h, &t));
  TEST_ASSERT_EQUAL_UINT32(10, h);
}

void test_get_rejects_bad_id(void) {
  const ServiceReport r = report(1, 2, 3, 4);
  ServiceState_Apply(&r);
  uint32_t h, t;
  TEST_ASSERT_FALSE(ServiceState_Get(SVC_COUNT, &h, &t));
  TEST_ASSERT_FALSE(ServiceState_IsDue(SVC_COUNT));
}

// ---------------- Vencimiento ----------------

void test_due_when_hours_reach_threshold(void) {
  const ServiceReport r = report(4999, 10000, 2500, 0);
  ServiceState_Apply(&r);
  TEST_ASSERT_FALSE(ServiceState_IsDue(SVC_HEATER));
  TEST_ASSERT_TRUE(ServiceState_IsDue(SVC_FAN));
  TEST_ASSERT_TRUE(ServiceState_IsDue(SVC_HUMIDIFIER));
  TEST_ASSERT_FALSE(ServiceState_IsDue(SVC_PHOTO));
  TEST_ASSERT_TRUE(ServiceState_AnyDue());
  TEST_ASSERT_EQUAL_UINT8(2, ServiceState_DueCount());
}

void test_zero_threshold_is_never_due(void) {
  ServiceReport r = report(99999, 0, 0, 0);
  r.entry[0].thresholdH = 0;
  ServiceState_Apply(&r);
  TEST_ASSERT_FALSE(ServiceState_IsDue(SVC_HEATER));
  TEST_ASSERT_FALSE(ServiceState_AnyDue());
}

// ---------------- Aviso y "MAS TARDE" ----------------

void test_warns_when_due_and_not_snoozed(void) {
  const ServiceReport r = report(0, 0, 2000, 0);
  ServiceState_Apply(&r);
  TEST_ASSERT_TRUE(ServiceState_ShouldWarn(5000));
}

void test_snooze_silences_for_24h(void) {
  const ServiceReport r = report(0, 0, 2000, 0);
  ServiceState_Apply(&r);
  ServiceState_Snooze(5000);
  TEST_ASSERT_FALSE(ServiceState_ShouldWarn(5000));
  TEST_ASSERT_FALSE(ServiceState_ShouldWarn(5000 + DAY - 1));
  TEST_ASSERT_TRUE(ServiceState_ShouldWarn(5000 + DAY));
}

// Callar el aviso del humidificador no debe esconder que ahora tambien ha
// vencido el ventilador: es otra cosa que el operador no ha visto.
void test_new_due_actuator_breaks_the_snooze(void) {
  const ServiceReport r1 = report(0, 0, 2000, 0);
  ServiceState_Apply(&r1);
  ServiceState_Snooze(5000);
  const ServiceReport r2 = report(0, 10000, 2001, 0);
  ServiceState_Apply(&r2);
  TEST_ASSERT_TRUE(ServiceState_ShouldWarn(6000));
}

void test_service_done_clears_the_warning(void) {
  const ServiceReport r1 = report(0, 0, 2000, 0);
  ServiceState_Apply(&r1);
  TEST_ASSERT_TRUE(ServiceState_ShouldWarn(10));
  const ServiceReport r2 = report(0, 0, 0, 0);
  ServiceState_Apply(&r2);
  TEST_ASSERT_FALSE(ServiceState_ShouldWarn(10));
}

void test_snooze_with_nothing_due_is_harmless(void) {
  const ServiceReport r = report(1, 1, 1, 1);
  ServiceState_Apply(&r);
  ServiceState_Snooze(100);
  const ServiceReport r2 = report(1, 1, 2000, 1);
  ServiceState_Apply(&r2);
  TEST_ASSERT_TRUE(ServiceState_ShouldWarn(101));
}

// ---------------- De la linea al estado ----------------

void test_line_to_state(void) {
  ServiceReport r;
  TEST_ASSERT_TRUE(service_proto_parse_report(
      "CTRL,SERVICE,4,0,10,5000,1,10000,10000,2,5,2000,3,7,10000", &r));
  ServiceState_Apply(&r);
  TEST_ASSERT_TRUE(ServiceState_IsDue(SVC_FAN));
  TEST_ASSERT_FALSE(ServiceState_IsDue(SVC_HEATER));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_no_data_before_first_report);
  RUN_TEST(test_apply_stores_hours_and_threshold);
  RUN_TEST(test_partial_report_keeps_other_actuators);
  RUN_TEST(test_get_rejects_bad_id);
  RUN_TEST(test_due_when_hours_reach_threshold);
  RUN_TEST(test_zero_threshold_is_never_due);
  RUN_TEST(test_warns_when_due_and_not_snoozed);
  RUN_TEST(test_snooze_silences_for_24h);
  RUN_TEST(test_new_due_actuator_breaks_the_snooze);
  RUN_TEST(test_service_done_clears_the_warning);
  RUN_TEST(test_snooze_with_nothing_due_is_harmless);
  RUN_TEST(test_line_to_state);
  return UNITY_END();
}

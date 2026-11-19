#include <math.h>
#include <unity.h>

#include "modules/service/service_calc.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------- Horas desde servicio ----------------

void test_since_is_life_minus_base(void) {
  TEST_ASSERT_EQUAL_FLOAT(30.5f, service_hours_since(4230.5f, 4200.0f));
}

void test_since_without_base_is_whole_life(void) {
  TEST_ASSERT_EQUAL_FLOAT(123.25f, service_hours_since(123.25f, 0.0f));
}

// Contador de vida reiniciado (NVS borrada, migracion): la base ya no vale y
// no puede salir un negativo ni un desbordamiento al pasar a entero.
void test_since_with_life_below_base_uses_life(void) {
  TEST_ASSERT_EQUAL_FLOAT(12.0f, service_hours_since(12.0f, 4200.0f));
}

void test_since_never_negative_or_nan(void) {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, service_hours_since(-5.0f, 0.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, service_hours_since(NAN, 10.0f));
  // Una base corrupta se ignora, no envenena el calculo.
  TEST_ASSERT_EQUAL_FLOAT(50.0f, service_hours_since(50.0f, NAN));
  TEST_ASSERT_EQUAL_FLOAT(50.0f, service_hours_since(50.0f, -3.0f));
}

void test_whole_hours_truncates_down(void) {
  TEST_ASSERT_EQUAL_UINT32(1999, service_whole_hours(1999.9f));
  TEST_ASSERT_EQUAL_UINT32(0, service_whole_hours(0.99f));
  TEST_ASSERT_EQUAL_UINT32(0, service_whole_hours(-1.0f));
}

void test_whole_hours_saturates(void) {
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, service_whole_hours(1.0e12f));
}

// ---------------- Vencimiento ----------------

void test_due_at_threshold(void) {
  TEST_ASSERT_FALSE(service_is_due(1999, 2000));
  TEST_ASSERT_TRUE(service_is_due(2000, 2000));
  TEST_ASSERT_TRUE(service_is_due(2500, 2000));
}

void test_zero_threshold_disables(void) {
  TEST_ASSERT_FALSE(service_is_due(0, 0));
  TEST_ASSERT_FALSE(service_is_due(99999, 0));
}

// ---------------- Informe ----------------

void test_report_has_all_actuators_in_id_order(void) {
  const float life[SVC_COUNT] = {100.0f, 4230.5f, 2000.9f, 7.0f};
  const float base[SVC_COUNT] = {0.0f, 4200.0f, 0.0f, 0.0f};
  const uint32_t thr[SVC_COUNT] = {5000, 10000, 2000, 0};
  ServiceReport r;
  service_calc_report(life, base, thr, &r);
  TEST_ASSERT_EQUAL_UINT8(SVC_COUNT, r.n);
  for (uint8_t i = 0; i < SVC_COUNT; i++) {
    TEST_ASSERT_EQUAL_UINT8(i, r.entry[i].id);
  }
  TEST_ASSERT_EQUAL_UINT32(100, r.entry[SVC_HEATER].hours);
  TEST_ASSERT_EQUAL_UINT32(30, r.entry[SVC_FAN].hours);
  TEST_ASSERT_EQUAL_UINT32(2000, r.entry[SVC_HUMIDIFIER].hours);
  TEST_ASSERT_EQUAL_UINT32(5000, r.entry[SVC_HEATER].thresholdH);
  TEST_ASSERT_EQUAL_UINT32(0, r.entry[SVC_PHOTO].thresholdH);
}

void test_report_tolerates_null(void) {
  const float z[SVC_COUNT] = {0, 0, 0, 0};
  const uint32_t t[SVC_COUNT] = {1, 1, 1, 1};
  ServiceReport r;
  service_calc_report(nullptr, z, t, &r);
  TEST_ASSERT_EQUAL_UINT8(0, r.n);
  service_calc_report(z, z, t, nullptr);  // no debe caerse
}

// ---------------- Umbrales compilados ----------------

void test_default_thresholds_are_the_documented_ones(void) {
  TEST_ASSERT_EQUAL_UINT32(5000, service_threshold_hours(SVC_HEATER));
  TEST_ASSERT_EQUAL_UINT32(10000, service_threshold_hours(SVC_FAN));
  TEST_ASSERT_EQUAL_UINT32(2000, service_threshold_hours(SVC_HUMIDIFIER));
  TEST_ASSERT_EQUAL_UINT32(10000, service_threshold_hours(SVC_PHOTO));
  TEST_ASSERT_EQUAL_UINT32(0, service_threshold_hours(SVC_COUNT));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_since_is_life_minus_base);
  RUN_TEST(test_since_without_base_is_whole_life);
  RUN_TEST(test_since_with_life_below_base_uses_life);
  RUN_TEST(test_since_never_negative_or_nan);
  RUN_TEST(test_whole_hours_truncates_down);
  RUN_TEST(test_whole_hours_saturates);
  RUN_TEST(test_due_at_threshold);
  RUN_TEST(test_zero_threshold_disables);
  RUN_TEST(test_report_has_all_actuators_in_id_order);
  RUN_TEST(test_report_tolerates_null);
  RUN_TEST(test_default_thresholds_are_the_documented_ones);
  return UNITY_END();
}

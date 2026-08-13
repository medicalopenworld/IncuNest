#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "modules/baby_profile/baby_cloud.h"

void setUp(void) {}
void tearDown(void) {}

static BabyProfile mk() {
  BabyProfile p;
  memset(&p, 0, sizeof(p));
  p.slotUsed = true;
  p.seq = 7;
  snprintf(p.name, BABY_NAME_LEN, "%s", "ANA");
  p.gestWeeks = 31;
  p.weightGrams = 1450;
  p.admissionEpoch = 1700000000u;
  p.kangarooCount = 4;
  p.phototherapyMinutes = 90;
  p.thermoMinutes = 300;
  return p;
}

// --- The whole point of the redesign: every payload identifies the baby ---

void test_every_payload_carries_baby_seq(void) {
  BabyProfile p = mk();
  char buf[512];

  BabyCloudEvent w = {BABY_EVT_WEIGHT, 1700000500u, 1460, p};
  TEST_ASSERT_GREATER_THAN_INT(0, babyCloud_buildEventJson(&w, buf, sizeof(buf)));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_seq\":7"));

  BabyCloudEvent k = {BABY_EVT_KANGAROO, 1700000600u, 0, p};
  TEST_ASSERT_GREATER_THAN_INT(0, babyCloud_buildEventJson(&k, buf, sizeof(buf)));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_seq\":7"));

  BabyCloudEvent d = {BABY_EVT_DISCHARGE, 1700000700u, 1460, p};
  TEST_ASSERT_GREATER_THAN_INT(0, babyCloud_buildEventJson(&d, buf, sizeof(buf)));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_seq\":7"));

  TEST_ASSERT_GREATER_THAN_INT(
      0, babyCloud_buildAttributesJson(&p, buf, sizeof(buf)));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_seq\":7"));
}

// --- Timestamps: real event time, not publish time ---

void test_known_timestamp_uses_ts_envelope_in_millis(void) {
  BabyProfile p = mk();
  BabyCloudEvent w = {BABY_EVT_WEIGHT, 1700000500u, 1460, p};
  char buf[512];
  babyCloud_buildEventJson(&w, buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"ts\":1700000500000"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"values\":{"));
}

void test_unsynced_clock_omits_ts_so_server_stamps_it(void) {
  BabyProfile p = mk();
  BabyCloudEvent w = {BABY_EVT_WEIGHT, 0, 1460, p};
  char buf[512];
  babyCloud_buildEventJson(&w, buf, sizeof(buf));
  // No ts envelope at all, and no bogus 1970 timestamp.
  TEST_ASSERT_NULL(strstr(buf, "\"ts\""));
  TEST_ASSERT_EQUAL_STRING("{\"baby_seq\":7,\"baby_weight_g\":1460}", buf);
}

// --- Discharge row must stand alone (history table reads only this) ---

void test_discharge_is_self_contained_with_stay_days(void) {
  BabyProfile p = mk();
  p.dischargeEpoch = p.admissionEpoch + 5u * 86400u;
  p.outcome = BABY_OUTCOME_SURVIVED;
  BabyCloudEvent d = {BABY_EVT_DISCHARGE, p.dischargeEpoch, 1460, p};
  char buf[512];
  TEST_ASSERT_GREATER_THAN_INT(0, babyCloud_buildEventJson(&d, buf, sizeof(buf)));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_name\":\"ANA\""));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_gest_weeks\":31"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_outcome\":1"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_kangaroo_count\":4"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_phototherapy_min\":90"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_thermo_min\":300"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_stay_days\":5"));
}

void test_stay_days_omitted_when_dates_unusable(void) {
  BabyProfile p = mk();
  p.admissionEpoch = 0;  // clock was never synced during the stay
  p.dischargeEpoch = 1700000000u;
  BabyCloudEvent d = {BABY_EVT_DISCHARGE, p.dischargeEpoch, 0, p};
  char buf[512];
  babyCloud_buildEventJson(&d, buf, sizeof(buf));
  // Better absent than a fabricated 19000-day stay.
  TEST_ASSERT_NULL(strstr(buf, "baby_stay_days"));
}

// --- Robustness ---

void test_name_with_quotes_cannot_break_the_json(void) {
  BabyProfile p = mk();
  snprintf(p.name, BABY_NAME_LEN, "%s", "A\"B\\C");
  char buf[512];
  TEST_ASSERT_GREATER_THAN_INT(
      0, babyCloud_buildAttributesJson(&p, buf, sizeof(buf)));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\\\"B"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\\\\C"));
  // Braces stay balanced: one open, one close.
  int depth = 0, minDepth = 0;
  for (const char *c = buf; *c; c++) {
    if (*c == '{') depth++;
    if (*c == '}') depth--;
    if (depth < minDepth) minDepth = depth;
  }
  TEST_ASSERT_EQUAL_INT(0, depth);
  TEST_ASSERT_EQUAL_INT(0, minDepth);
}

void test_empty_attributes_clear_every_occupancy_key(void) {
  char buf[512];
  TEST_ASSERT_GREATER_THAN_INT(
      0, babyCloud_buildEmptyAttributesJson(buf, sizeof(buf)));
  // Omitting a key would leave the discharged baby on the dashboard card.
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_seq\":0"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_name\":\"\""));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_kangaroo_count\":0"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"baby_thermo_min\":0"));
}

void test_small_buffer_fails_closed(void) {
  BabyProfile p = mk();
  BabyCloudEvent d = {BABY_EVT_DISCHARGE, 1700000700u, 1460, p};
  char buf[24];
  // 0 means "do not send", never a truncated half-JSON.
  TEST_ASSERT_EQUAL_INT(0, babyCloud_buildEventJson(&d, buf, sizeof(buf)));
}

void test_unknown_event_type_builds_nothing(void) {
  BabyProfile p = mk();
  BabyCloudEvent e = {BABY_EVT_NONE, 1700000700u, 0, p};
  char buf[128];
  TEST_ASSERT_EQUAL_INT(0, babyCloud_buildEventJson(&e, buf, sizeof(buf)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_every_payload_carries_baby_seq);
  RUN_TEST(test_known_timestamp_uses_ts_envelope_in_millis);
  RUN_TEST(test_unsynced_clock_omits_ts_so_server_stamps_it);
  RUN_TEST(test_discharge_is_self_contained_with_stay_days);
  RUN_TEST(test_stay_days_omitted_when_dates_unusable);
  RUN_TEST(test_name_with_quotes_cannot_break_the_json);
  RUN_TEST(test_empty_attributes_clear_every_occupancy_key);
  RUN_TEST(test_small_buffer_fails_closed);
  RUN_TEST(test_unknown_event_type_builds_nothing);
  return UNITY_END();
}

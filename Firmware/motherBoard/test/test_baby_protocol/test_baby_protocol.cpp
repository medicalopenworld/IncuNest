#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "modules/baby_profile/baby_profile_protocol.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------- Parse: valid messages ----------------

void test_parse_list_req(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_LIST_REQ,
                    baby_proto_parse("HMI,PROFILE_LIST_REQ", &m));
}

void test_parse_new(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_NEW,
                    baby_proto_parse("HMI,PROFILE_NEW,Garcia,31", &m));
  TEST_ASSERT_EQUAL_STRING("Garcia", m.name);
  TEST_ASSERT_EQUAL_UINT8(31, m.gestWeeks);
}

void test_parse_select(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_SELECT,
                    baby_proto_parse("HMI,PROFILE_SELECT,42", &m));
  TEST_ASSERT_EQUAL_UINT32(42, m.seq);
}

void test_parse_weight_value_and_skip(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_WEIGHT,
                    baby_proto_parse("HMI,PROFILE_WEIGHT,42,1450", &m));
  TEST_ASSERT_EQUAL_UINT32(42, m.seq);
  TEST_ASSERT_EQUAL_UINT16(1450, m.grams);

  TEST_ASSERT_EQUAL(BABY_MSG_WEIGHT,
                    baby_proto_parse("HMI,PROFILE_WEIGHT,42,SKIP", &m));
  TEST_ASSERT_EQUAL_UINT16(0, m.grams);
}

void test_parse_age_manual(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_AGE_MANUAL,
                    baby_proto_parse("HMI,PROFILE_AGE_MANUAL,42,9", &m));
  TEST_ASSERT_EQUAL_UINT16(9, m.ageDays);
}

void test_parse_discharge_all_outcomes(void) {
  BabyProtoMsg m;
  for (int oc = 0; oc <= 3; oc++) {
    char line[48];
    snprintf(line, sizeof(line), "HMI,PROFILE_DISCHARGE,42,%d,0", oc);
    TEST_ASSERT_EQUAL(BABY_MSG_DISCHARGE, baby_proto_parse(line, &m));
    TEST_ASSERT_EQUAL_UINT8(oc, m.outcome);
    TEST_ASSERT_EQUAL_UINT8(0, m.cause);
  }
}

void test_parse_discharge_all_causes(void) {
  BabyProtoMsg m;
  for (int cause = 0; cause <= 6; cause++) {
    char line[48];
    snprintf(line, sizeof(line), "HMI,PROFILE_DISCHARGE,42,2,%d", cause);
    TEST_ASSERT_EQUAL(BABY_MSG_DISCHARGE, baby_proto_parse(line, &m));
    TEST_ASSERT_EQUAL_UINT8(cause, m.cause);
  }
}

void test_parse_kangaroo(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_KANGAROO,
                    baby_proto_parse("HMI,PROFILE_KANGAROO,42", &m));
  TEST_ASSERT_EQUAL_UINT32(42, m.seq);
}

void test_parse_kangaroo_rejects_malformed(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_KANGAROO", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_KANGAROO,x", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_KANGAROO,42,1", &m));
}

void test_parse_history_req(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_HISTORY_REQ,
                    baby_proto_parse("HMI,PROFILE_HISTORY_REQ,3", &m));
  TEST_ASSERT_EQUAL_UINT32(3, m.page);
}

void test_parse_weight_history_req(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_WEIGHT_HISTORY_REQ,
                    baby_proto_parse("HMI,WEIGHT_HISTORY_REQ,42", &m));
  TEST_ASSERT_EQUAL_UINT32(42, m.seq);
}

// ---------------- Parse: malformed lines are silently discarded ----------

void test_parse_rejects_short_field_count(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_NONE, baby_proto_parse("HMI,PROFILE_NEW", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_NEW,Garcia", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_WEIGHT,42", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_DISCHARGE,42", &m));
  // Pre-v2.2.0 3-field form (no cause) is now short by exactly one field.
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_DISCHARGE,42,1", &m));
}

void test_parse_rejects_non_numeric_fields(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_SELECT,abc", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_WEIGHT,42,12x0", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_NEW,Garcia,3x", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_HISTORY_REQ,", &m));
}

void test_parse_rejects_out_of_range_outcome(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_DISCHARGE,42,4,0", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_DISCHARGE,42,-1,0", &m));
}

void test_parse_rejects_out_of_range_cause(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_DISCHARGE,42,2,7", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE,
                    baby_proto_parse("HMI,PROFILE_DISCHARGE,42,2,-1", &m));
}

void test_parse_rejects_empty_name(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_NONE, baby_proto_parse("HMI,PROFILE_NEW,,31", &m));
}

void test_parse_rejects_unknown_and_non_profile_lines(void) {
  BabyProtoMsg m;
  TEST_ASSERT_EQUAL(BABY_MSG_NONE, baby_proto_parse("HMI,PROFILE_NOPE,1", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE, baby_proto_parse("HMI,1,0,1", &m));
  TEST_ASSERT_EQUAL(BABY_MSG_NONE, baby_proto_parse("", &m));
}

// ---------------- Builders ----------------

static BabyProfile mkProfile(uint32_t seq, const char *name, uint8_t gest,
                             uint16_t weight) {
  BabyProfile p;
  memset(&p, 0, sizeof(p));
  p.slotUsed = true;
  p.seq = seq;
  snprintf(p.name, BABY_NAME_LEN, "%s", name);
  p.gestWeeks = gest;
  p.weightGrams = weight;
  return p;
}

void test_build_list_with_two_profiles(void) {
  BabyProfile slots[BABY_ACTIVE_SLOTS];
  memset(slots, 0, sizeof(slots));
  slots[0] = mkProfile(7, "Ana", 30, 1200);
  slots[0].kangarooCount = 2;
  slots[0].phototherapyMinutes = 45;
  slots[0].thermoMinutes = 600;
  slots[0].humidityMinutes = 120;
  slots[2] = mkProfile(9, "Luca", 33, 0);

  char buf[256];
  int n = baby_proto_build_list(buf, sizeof(buf), slots);
  TEST_ASSERT_GREATER_THAN_INT(0, n);
  TEST_ASSERT_EQUAL_STRING(
      "CTRL,PROFILE_LIST,2,7,Ana,30,1200,2,45,600,120,9,Luca,33,0,0,0,0,0\n",
      buf);
}

void test_build_list_empty(void) {
  BabyProfile slots[BABY_ACTIVE_SLOTS];
  memset(slots, 0, sizeof(slots));
  char buf[64];
  baby_proto_build_list(buf, sizeof(buf), slots);
  TEST_ASSERT_EQUAL_STRING("CTRL,PROFILE_LIST,0\n", buf);
}

void test_build_range_known_age(void) {
  NteRange r = {33.0f, 34.5f, 33.75f, false};
  char buf[96];
  baby_proto_build_range(buf, sizeof(buf), 42, true, 9, &r);
  TEST_ASSERT_EQUAL_STRING("CTRL,PROFILE_RANGE,42,1,9,33.0,34.5,33.8,0\n",
                           buf);
}

void test_build_range_unknown_age_sentinel(void) {
  NteRange r = {-1.0f, -1.0f, -1.0f, true};
  char buf[96];
  baby_proto_build_range(buf, sizeof(buf), 42, false, 0, &r);
  TEST_ASSERT_EQUAL_STRING("CTRL,PROFILE_RANGE,42,0,0,-1.0,-1.0,-1.0,1\n",
                           buf);
}

void test_build_history_page(void) {
  BabyProfile recs[2];
  recs[0] = mkProfile(9, "Luca", 33, 1800);
  recs[0].admissionEpoch = 1000;
  recs[0].dischargeEpoch = 2000;
  recs[0].outcome = BABY_OUTCOME_SURVIVED;
  recs[0].kangarooCount = 4;
  recs[0].phototherapyMinutes = 90;
  recs[0].thermoMinutes = 300;
  recs[0].humidityMinutes = 240;
  recs[1] = mkProfile(7, "Ana", 30, 1200);

  char buf[256];
  baby_proto_build_history(buf, sizeof(buf), 0, 12, recs, 2);
  TEST_ASSERT_EQUAL_STRING(
      "CTRL,PROFILE_HISTORY,0,12,2,9,Luca,33,1800,1000,2000,1,0,4,90,300,240,"
      "7,Ana,30,1200,0,0,0,0,0,0,0,0\n",
      buf);
}

void test_build_weight_history(void) {
  BabyWeightPoint pts[3] = {{0, 1200}, {2, 1180}, {5, 1260}};
  char buf[128];
  baby_proto_build_weight_history(buf, sizeof(buf), 42, pts, 3);
  TEST_ASSERT_EQUAL_STRING(
      "CTRL,WEIGHT_HISTORY,42,3,0,1200,2,1180,5,1260\n", buf);
}

void test_build_weight_history_max_size_fits_1024(void) {
  // Worst case: 50 points, all fields at max width.
  BabyWeightPoint pts[BABY_WEIGHT_HISTORY_MAX_OUT];
  for (uint32_t i = 0; i < BABY_WEIGHT_HISTORY_MAX_OUT; i++) {
    pts[i].dayOffset = 65535;
    pts[i].weightGrams = 65535;
  }
  char buf[1024];
  int n = baby_proto_build_weight_history(buf, sizeof(buf), 4294967295u, pts,
                                          BABY_WEIGHT_HISTORY_MAX_OUT);
  TEST_ASSERT_GREATER_THAN_INT(0, n);
  TEST_ASSERT_LESS_THAN_INT(1024, n);
}

void test_build_returns_zero_when_buffer_too_small(void) {
  BabyWeightPoint pts[2] = {{0, 1200}, {2, 1180}};
  char buf[16];
  TEST_ASSERT_EQUAL_INT(
      0, baby_proto_build_weight_history(buf, sizeof(buf), 42, pts, 2));
}

void test_build_history_worst_case_fits_response_buffer(void) {
  // Mirrors s_babyRespBuf (CommTask): a full page with every field at
  // max width must still build, otherwise the page silently vanishes.
  BabyProfile recs[10];
  for (int i = 0; i < 10; i++) {
    memset(&recs[i], 0, sizeof(recs[i]));
    recs[i].slotUsed = true;
    recs[i].seq = 4294967295u;
    memset(recs[i].name, 88, BABY_NAME_LEN - 1);
    recs[i].name[BABY_NAME_LEN - 1] = 0;
    recs[i].gestWeeks = 255;
    recs[i].weightGrams = 65535;
    recs[i].admissionEpoch = 4294967295u;
    recs[i].dischargeEpoch = 4294967295u;
    recs[i].outcome = 3;
    recs[i].cause = 255;
    recs[i].kangarooCount = 65535;
    recs[i].phototherapyMinutes = 4294967295u;
    recs[i].thermoMinutes = 4294967295u;
    recs[i].humidityMinutes = 4294967295u;
  }
  char buf[1280];
  int n = baby_proto_build_history(buf, sizeof(buf), 4294967295u,
                                   4294967295u, recs, 10);
  TEST_ASSERT_GREATER_THAN_INT(0, n);
  TEST_ASSERT_LESS_THAN_INT(1280, n);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_list_req);
  RUN_TEST(test_parse_new);
  RUN_TEST(test_parse_select);
  RUN_TEST(test_parse_weight_value_and_skip);
  RUN_TEST(test_parse_age_manual);
  RUN_TEST(test_parse_discharge_all_outcomes);
  RUN_TEST(test_parse_discharge_all_causes);
  RUN_TEST(test_parse_kangaroo);
  RUN_TEST(test_parse_kangaroo_rejects_malformed);
  RUN_TEST(test_parse_history_req);
  RUN_TEST(test_parse_weight_history_req);
  RUN_TEST(test_parse_rejects_short_field_count);
  RUN_TEST(test_parse_rejects_non_numeric_fields);
  RUN_TEST(test_parse_rejects_out_of_range_outcome);
  RUN_TEST(test_parse_rejects_out_of_range_cause);
  RUN_TEST(test_parse_rejects_empty_name);
  RUN_TEST(test_parse_rejects_unknown_and_non_profile_lines);
  RUN_TEST(test_build_list_with_two_profiles);
  RUN_TEST(test_build_list_empty);
  RUN_TEST(test_build_range_known_age);
  RUN_TEST(test_build_range_unknown_age_sentinel);
  RUN_TEST(test_build_history_page);
  RUN_TEST(test_build_history_worst_case_fits_response_buffer);
  RUN_TEST(test_build_weight_history);
  RUN_TEST(test_build_weight_history_max_size_fits_1024);
  RUN_TEST(test_build_returns_zero_when_buffer_too_small);
  return UNITY_END();
}

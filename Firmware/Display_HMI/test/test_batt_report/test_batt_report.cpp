#include <unity.h>

#include "modules/comm/batt_report.h"

void setUp(void) {}
void tearDown(void) {}

// Estado "centinela" para comprobar que un rechazo NO toca la salida.
static BattRepMsg poisoned(void) {
  BattRepMsg m;
  m.state = 77;
  m.level = 88;
  m.pct = 99;
  return m;
}

static void assert_untouched(const BattRepMsg &m) {
  TEST_ASSERT_EQUAL_INT(77, m.state);
  TEST_ASSERT_EQUAL_INT(88, m.level);
  TEST_ASSERT_EQUAL_INT(99, m.pct);
}

// ---- Parser -----------------------------------------------------------------

void test_parse_valid_line(void) {
  BattRepMsg m = poisoned();
  TEST_ASSERT_TRUE(battrep_parse("CTRL,BATT,1,3,62", &m));
  TEST_ASSERT_EQUAL_INT(PROTO_BATT_STATE_DISCHARGING, m.state);
  TEST_ASSERT_EQUAL_INT(3, m.level);
  TEST_ASSERT_EQUAL_INT(62, m.pct);
}

void test_parse_accepts_trailing_fields(void) {
  BattRepMsg m = poisoned();
  TEST_ASSERT_TRUE(battrep_parse("CTRL,BATT,3,4,100,12800", &m));
  TEST_ASSERT_EQUAL_INT(PROTO_BATT_STATE_FULL, m.state);
}

void test_parse_rejects_malformed(void) {
  const char *bad[] = {
      "CTRL,BATT,1,3",       // falta un campo
      "CTRL,BATT,1,3,",      // campo vacio
      "CTRL,BATT,4,3,50",    // estado fuera de rango
      "CTRL,BATT,1,5,50",    // demasiadas rayas
      "CTRL,BATT,1,-1,50",   // rayas negativas
      "CTRL,BATT,1,3,101",   // porcentaje fuera de rango
      "CTRL,BATT,1,x,50",    // no numerico
      "CTRL,ENV,1,0,1,40",   // otra linea
      "",
  };
  for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    BattRepMsg m = poisoned();
    TEST_ASSERT_FALSE_MESSAGE(battrep_parse(bad[i], &m), bad[i]);
    assert_untouched(m);
  }
  TEST_ASSERT_FALSE(battrep_parse(nullptr, nullptr));
}

// ---- Vista ------------------------------------------------------------------

static BattRepMsg msg(int state, int level, int pct) {
  BattRepMsg m = {state, level, pct};
  return m;
}

void test_view_nothing_without_a_line(void) {
  BattRepMsg m = msg(PROTO_BATT_STATE_FULL, 4, 100);
  TEST_ASSERT_EQUAL(BATTREP_SHOW_NONE,
                    battrep_view(false, &m, 0, 1000, false).show);
}

void test_view_nothing_when_stale_or_link_lost(void) {
  BattRepMsg m = msg(PROTO_BATT_STATE_DISCHARGING, 2, 40);
  TEST_ASSERT_EQUAL(BATTREP_SHOW_NONE,
                    battrep_view(true, &m, 0, BATTREP_STALE_MS + 1, false).show);
  TEST_ASSERT_EQUAL(BATTREP_SHOW_NONE,
                    battrep_view(true, &m, 0, 1000, true).show);
}

void test_view_states(void) {
  BattRepMsg d = msg(PROTO_BATT_STATE_DISCHARGING, 2, 40);
  BattRepView v = battrep_view(true, &d, 0, 1000, false);
  TEST_ASSERT_EQUAL(BATTREP_SHOW_DISCHARGING, v.show);
  TEST_ASSERT_EQUAL_INT(2, v.level);

  BattRepMsg c = msg(PROTO_BATT_STATE_CHARGING, 1, 20);
  v = battrep_view(true, &c, 0, 1000, false);
  TEST_ASSERT_EQUAL(BATTREP_SHOW_CHARGING, v.show);
  TEST_ASSERT_EQUAL_INT(0, v.level);

  BattRepMsg f = msg(PROTO_BATT_STATE_FULL, 4, 100);
  v = battrep_view(true, &f, 0, 1000, false);
  TEST_ASSERT_EQUAL(BATTREP_SHOW_FULL, v.show);
  TEST_ASSERT_EQUAL_INT(PROTO_BATT_LEVEL_MAX, v.level);

  BattRepMsg u = msg(PROTO_BATT_STATE_UNKNOWN, 0, 0);
  TEST_ASSERT_EQUAL(BATTREP_SHOW_NONE,
                    battrep_view(true, &u, 0, 1000, false).show);
}

// Cargando: rayas fijas por cada cuarto completo; la cuarta solo en FULL.
void test_view_charging_shows_completed_quarters(void) {
  const int pcts[] = {0, 24, 25, 60, 74, 75, 99};
  const int bars[] = {0, 0, 1, 2, 2, 3, 3};
  for (unsigned i = 0; i < sizeof(pcts) / sizeof(pcts[0]); i++) {
    BattRepMsg c = msg(PROTO_BATT_STATE_CHARGING, 0, pcts[i]);
    TEST_ASSERT_EQUAL_INT(bars[i], battrep_view(true, &c, 0, 1000, false).level);
  }
}

void test_view_survives_millis_wraparound(void) {
  BattRepMsg m = msg(PROTO_BATT_STATE_DISCHARGING, 3, 60);
  TEST_ASSERT_EQUAL(BATTREP_SHOW_DISCHARGING,
                    battrep_view(true, &m, 0xFFFFF000u, 0x00000100u, false).show);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_valid_line);
  RUN_TEST(test_parse_accepts_trailing_fields);
  RUN_TEST(test_parse_rejects_malformed);
  RUN_TEST(test_view_nothing_without_a_line);
  RUN_TEST(test_view_nothing_when_stale_or_link_lost);
  RUN_TEST(test_view_states);
  RUN_TEST(test_view_charging_shows_completed_quarters);
  RUN_TEST(test_view_survives_millis_wraparound);
  return UNITY_END();
}

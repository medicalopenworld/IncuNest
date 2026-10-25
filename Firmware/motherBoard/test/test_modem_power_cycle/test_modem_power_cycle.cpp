// Temporizacion del power-cycle del modem (V19). Ver
// modules/control/modem_power_cycle.h.

#include <unity.h>

#include "modules/control/modem_power_cycle.h"

static ModemPowerCycle pc;
static uint32_t t0;

void setUp(void) {
  modem_pc_init(&pc);
  t0 = 100000; // lejos de 0
}
void tearDown(void) {}

void test_idle_does_nothing(void) {
  TEST_ASSERT_FALSE(modem_pc_busy(&pc));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_NONE, modem_pc_poll(&pc, t0));
  TEST_ASSERT_EQUAL_UINT32(0, modem_pc_count(&pc));
}

void test_cut_only_after_shutdown_grace(void) {
  TEST_ASSERT_TRUE(modem_pc_begin(&pc, t0, MODEM_PC_SHUTDOWN_GRACE_MS));
  TEST_ASSERT_TRUE(modem_pc_busy(&pc));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_NONE, modem_pc_poll(&pc, t0));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_NONE,
                    modem_pc_poll(&pc, t0 + MODEM_PC_SHUTDOWN_GRACE_MS - 1));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_CUT,
                    modem_pc_poll(&pc, t0 + MODEM_PC_SHUTDOWN_GRACE_MS));
}

void test_cut_is_emitted_once(void) {
  modem_pc_begin(&pc, t0, 0);
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_CUT, modem_pc_poll(&pc, t0));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_NONE, modem_pc_poll(&pc, t0 + 1));
}

void test_restore_not_before_two_seconds_off(void) {
  modem_pc_begin(&pc, t0, MODEM_PC_SHUTDOWN_GRACE_MS);
  const uint32_t cut = t0 + MODEM_PC_SHUTDOWN_GRACE_MS;
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_CUT, modem_pc_poll(&pc, cut));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_NONE,
                    modem_pc_poll(&pc, cut + MODEM_PC_OFF_MIN_MS - 1));
  TEST_ASSERT_TRUE(modem_pc_busy(&pc));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_RESTORE,
                    modem_pc_poll(&pc, cut + MODEM_PC_OFF_MIN_MS));
  TEST_ASSERT_FALSE(modem_pc_busy(&pc));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_NONE,
                    modem_pc_poll(&pc, cut + MODEM_PC_OFF_MIN_MS + 1));
}

void test_off_time_counts_from_the_cut_not_from_begin(void) {
  // Si el llamante consulta tarde, el corte real es el del poll() que lo
  // ordena: los 2 s se cuentan desde ahi y no desde begin().
  modem_pc_begin(&pc, t0, MODEM_PC_SHUTDOWN_GRACE_MS);
  const uint32_t late = t0 + 5000;
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_CUT, modem_pc_poll(&pc, late));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_NONE, modem_pc_poll(&pc, late + 1999));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_RESTORE, modem_pc_poll(&pc, late + 2000));
}

void test_monitor_path_cuts_on_first_poll(void) {
  modem_pc_begin(&pc, t0, 0);
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_CUT, modem_pc_poll(&pc, t0));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_NONE,
                    modem_pc_poll(&pc, t0 + MODEM_PC_OFF_MIN_MS - 1));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_RESTORE,
                    modem_pc_poll(&pc, t0 + MODEM_PC_OFF_MIN_MS));
}

void test_begin_while_busy_is_rejected_and_not_counted(void) {
  TEST_ASSERT_TRUE(modem_pc_begin(&pc, t0, MODEM_PC_SHUTDOWN_GRACE_MS));
  TEST_ASSERT_FALSE(modem_pc_begin(&pc, t0 + 500, 0));
  TEST_ASSERT_EQUAL_UINT32(1, modem_pc_count(&pc));
  // El temporizador original no se movio: el corte sale a los 1500 ms de t0.
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_CUT,
                    modem_pc_poll(&pc, t0 + MODEM_PC_SHUTDOWN_GRACE_MS));
  // Y tampoco se acepta estando ya cortado.
  TEST_ASSERT_FALSE(modem_pc_begin(&pc, t0 + 1600, 0));
  TEST_ASSERT_EQUAL_UINT32(1, modem_pc_count(&pc));
}

void test_second_cycle_after_restore_is_counted(void) {
  modem_pc_begin(&pc, t0, 0);
  modem_pc_poll(&pc, t0);
  modem_pc_poll(&pc, t0 + MODEM_PC_OFF_MIN_MS);
  TEST_ASSERT_TRUE(modem_pc_begin(&pc, t0 + 10000, 0));
  TEST_ASSERT_EQUAL_UINT32(2, modem_pc_count(&pc));
}

void test_survives_millis_wraparound(void) {
  const uint32_t near_wrap = 0xFFFFFFFFu - 500u;
  modem_pc_begin(&pc, near_wrap, MODEM_PC_SHUTDOWN_GRACE_MS);
  // 1500 ms despues el contador ya dio la vuelta.
  const uint32_t cut = near_wrap + MODEM_PC_SHUTDOWN_GRACE_MS;
  TEST_ASSERT_TRUE(cut < near_wrap);
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_CUT, modem_pc_poll(&pc, cut));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_NONE,
                    modem_pc_poll(&pc, cut + MODEM_PC_OFF_MIN_MS - 1));
  TEST_ASSERT_EQUAL(MODEM_PC_ACTION_RESTORE,
                    modem_pc_poll(&pc, cut + MODEM_PC_OFF_MIN_MS));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_does_nothing);
  RUN_TEST(test_cut_only_after_shutdown_grace);
  RUN_TEST(test_cut_is_emitted_once);
  RUN_TEST(test_restore_not_before_two_seconds_off);
  RUN_TEST(test_off_time_counts_from_the_cut_not_from_begin);
  RUN_TEST(test_monitor_path_cuts_on_first_poll);
  RUN_TEST(test_begin_while_busy_is_rejected_and_not_counted);
  RUN_TEST(test_second_cycle_after_restore_is_counted);
  RUN_TEST(test_survives_millis_wraparound);
  return UNITY_END();
}

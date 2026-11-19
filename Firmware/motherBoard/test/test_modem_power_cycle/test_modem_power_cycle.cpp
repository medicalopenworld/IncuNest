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

// --- Politica de corte tras reinicios suaves fallidos (M5) ---
//
// Un corte de VBAT es drastico (el modem pierde registro en red y tarda en
// volver): solo tras MODEM_PC_SOFT_FAILS_BEFORE_CUT reinicios suaves seguidos
// sin llegar a conectar, y con backoff exponencial entre cortes.

// Pide `n` reinicios suaves seguidos y devuelve true si ALGUNO inicio un corte.
static bool soft_failures(uint32_t now, uint32_t n) {
  bool any = false;
  for (uint32_t i = 0; i < n; i++) {
    any |= modem_pc_soft_failure(&pc, now + i, MODEM_PC_SHUTDOWN_GRACE_MS);
  }
  return any;
}

// Completa el ciclo en curso (corte y restauracion) y devuelve el instante
// en que termina.
static uint32_t finish_cycle(uint32_t from) {
  uint32_t t = from;
  while (modem_pc_busy(&pc)) {
    t += 100;
    modem_pc_poll(&pc, t);
  }
  return t;
}

void test_soft_failures_below_threshold_do_not_cut(void) {
  for (uint32_t i = 0; i + 1 < MODEM_PC_SOFT_FAILS_BEFORE_CUT; i++) {
    TEST_ASSERT_FALSE(modem_pc_soft_failure(&pc, t0 + i, 1500));
  }
  TEST_ASSERT_FALSE(modem_pc_busy(&pc));
  TEST_ASSERT_EQUAL_UINT32(0, modem_pc_count(&pc));
  TEST_ASSERT_EQUAL_UINT32(MODEM_PC_SOFT_FAILS_BEFORE_CUT - 1,
                           modem_pc_soft_fails(&pc));
}

void test_threshold_failure_starts_a_cut(void) {
  TEST_ASSERT_TRUE(soft_failures(t0, MODEM_PC_SOFT_FAILS_BEFORE_CUT));
  TEST_ASSERT_TRUE(modem_pc_busy(&pc));
  TEST_ASSERT_EQUAL_UINT32(1, modem_pc_count(&pc));
  // El contador de suaves se vacia al cortar.
  TEST_ASSERT_EQUAL_UINT32(0, modem_pc_soft_fails(&pc));
}

void test_connecting_clears_the_soft_failure_streak(void) {
  soft_failures(t0, MODEM_PC_SOFT_FAILS_BEFORE_CUT - 1);
  modem_pc_connected(&pc);
  TEST_ASSERT_EQUAL_UINT32(0, modem_pc_soft_fails(&pc));
  TEST_ASSERT_FALSE(soft_failures(t0 + 10, MODEM_PC_SOFT_FAILS_BEFORE_CUT - 1));
  TEST_ASSERT_EQUAL_UINT32(0, modem_pc_count(&pc));
}

void test_backoff_blocks_a_second_cut_until_it_elapses(void) {
  TEST_ASSERT_TRUE(soft_failures(t0, MODEM_PC_SOFT_FAILS_BEFORE_CUT));
  const uint32_t done = finish_cycle(t0);
  // Sigue sin conectar: otra racha de fallos suaves, pero dentro del backoff.
  TEST_ASSERT_FALSE(soft_failures(done, MODEM_PC_SOFT_FAILS_BEFORE_CUT));
  TEST_ASSERT_FALSE(modem_pc_busy(&pc));
  // Cumplido el primer backoff (desde el corte) si se permite.
  TEST_ASSERT_TRUE(
      soft_failures(t0 + MODEM_PC_BACKOFF_MIN_MS + 1000,
                    MODEM_PC_SOFT_FAILS_BEFORE_CUT));
  TEST_ASSERT_EQUAL_UINT32(2, modem_pc_count(&pc));
}

void test_backoff_doubles_up_to_the_cap(void) {
  uint32_t t = t0;
  uint32_t expected = MODEM_PC_BACKOFF_MIN_MS;
  for (int i = 0; i < 12; i++) {
    TEST_ASSERT_TRUE(soft_failures(t, MODEM_PC_SOFT_FAILS_BEFORE_CUT));
    const uint32_t cut_at = t;
    t = finish_cycle(t);
    // Un instante antes de cumplirse el backoff vigente, no se corta.
    TEST_ASSERT_FALSE(soft_failures(cut_at + expected - 1000,
                                    MODEM_PC_SOFT_FAILS_BEFORE_CUT));
    t = cut_at + expected + 5; // y justo despues si
    expected = (expected * 2 > MODEM_PC_BACKOFF_MAX_MS)
                   ? MODEM_PC_BACKOFF_MAX_MS
                   : expected * 2;
    TEST_ASSERT_TRUE(modem_pc_backoff_ms(&pc) <= MODEM_PC_BACKOFF_MAX_MS);
  }
  TEST_ASSERT_EQUAL_UINT32(MODEM_PC_BACKOFF_MAX_MS, modem_pc_backoff_ms(&pc));
}

void test_connecting_resets_the_backoff(void) {
  soft_failures(t0, MODEM_PC_SOFT_FAILS_BEFORE_CUT);
  finish_cycle(t0);
  modem_pc_connected(&pc);
  TEST_ASSERT_EQUAL_UINT32(MODEM_PC_BACKOFF_MIN_MS, modem_pc_backoff_ms(&pc));
  // Sin backoff pendiente: la siguiente racha puede cortar enseguida.
  TEST_ASSERT_TRUE(soft_failures(t0 + 5000, MODEM_PC_SOFT_FAILS_BEFORE_CUT));
}

void test_soft_failure_while_busy_is_not_counted(void) {
  TEST_ASSERT_TRUE(modem_pc_begin(&pc, t0, 0)); // camino del monitor
  TEST_ASSERT_FALSE(modem_pc_soft_failure(&pc, t0 + 1, 1500));
  TEST_ASSERT_EQUAL_UINT32(0, modem_pc_soft_fails(&pc));
}

void test_forced_begin_ignores_the_policy(void) {
  // mon_kill: la tarea esta colgada, se corta en el acto aunque no haya
  // racha de fallos suaves ni haya pasado el backoff.
  TEST_ASSERT_TRUE(soft_failures(t0, MODEM_PC_SOFT_FAILS_BEFORE_CUT));
  const uint32_t done = finish_cycle(t0);
  TEST_ASSERT_TRUE(modem_pc_begin(&pc, done + 1, 0));
}

void test_backoff_gate_survives_millis_wraparound(void) {
  const uint32_t near_wrap = 0xFFFFFFFFu - 10000u;
  TEST_ASSERT_TRUE(soft_failures(near_wrap, MODEM_PC_SOFT_FAILS_BEFORE_CUT));
  finish_cycle(near_wrap);
  const uint32_t early = near_wrap + 20000u; // ya dio la vuelta, backoff vigente
  TEST_ASSERT_TRUE(early < near_wrap);
  TEST_ASSERT_FALSE(soft_failures(early, MODEM_PC_SOFT_FAILS_BEFORE_CUT));
  const uint32_t late = near_wrap + MODEM_PC_BACKOFF_MIN_MS + 1000u;
  TEST_ASSERT_TRUE(soft_failures(late, MODEM_PC_SOFT_FAILS_BEFORE_CUT));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_soft_failures_below_threshold_do_not_cut);
  RUN_TEST(test_threshold_failure_starts_a_cut);
  RUN_TEST(test_connecting_clears_the_soft_failure_streak);
  RUN_TEST(test_backoff_blocks_a_second_cut_until_it_elapses);
  RUN_TEST(test_backoff_doubles_up_to_the_cap);
  RUN_TEST(test_connecting_resets_the_backoff);
  RUN_TEST(test_soft_failure_while_busy_is_not_counted);
  RUN_TEST(test_forced_begin_ignores_the_policy);
  RUN_TEST(test_backoff_gate_survives_millis_wraparound);
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

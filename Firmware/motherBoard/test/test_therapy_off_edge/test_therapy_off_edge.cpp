#include <unity.h>

#include "modules/util/therapy_off_edge.h"

// Un instante cualquiera ya lejos del arranque, para que la separacion minima
// no se mezcle con el millis() pequeno de los primeros segundos.
#define T0 100000u
#define GAP THERAPY_OFF_EDGE_MIN_GAP_MS

static TherapyOffEdge st;

void setUp(void) { therapy_off_edge_init(&st); }
void tearDown(void) {}

// --- Deteccion del flanco -------------------------------------------------

// La primera observacion solo fija el estado de partida: un equipo que arranca
// con todo apagado no ha "apagado" nada.
void test_first_observation_is_not_an_edge(void) {
  therapy_off_edge_observe(&st, false, false);
  TEST_ASSERT_FALSE(therapy_off_edge_due(&st, T0));
}

void test_first_observation_with_everything_on_is_not_an_edge(void) {
  therapy_off_edge_observe(&st, true, true);
  TEST_ASSERT_FALSE(therapy_off_edge_due(&st, T0));
}

void test_phototherapy_turning_off_is_due(void) {
  therapy_off_edge_observe(&st, true, false);
  therapy_off_edge_observe(&st, false, false);
  TEST_ASSERT_TRUE(therapy_off_edge_due(&st, T0));
}

void test_control_turning_off_is_due(void) {
  therapy_off_edge_observe(&st, false, true);
  therapy_off_edge_observe(&st, false, false);
  TEST_ASSERT_TRUE(therapy_off_edge_due(&st, T0));
}

// Apagar una de las dos con la otra encendida tambien cuenta: el cambio de
// estado de la que se apaga es lo que ThingsBoard tiene que ver.
void test_one_turning_off_while_other_stays_on_is_due(void) {
  therapy_off_edge_observe(&st, true, true);
  therapy_off_edge_observe(&st, false, true);
  TEST_ASSERT_TRUE(therapy_off_edge_due(&st, T0));
}

// El encendido no fuerza envio (alcance acordado: solo el apagado).
void test_turning_on_is_not_due(void) {
  therapy_off_edge_observe(&st, false, false);
  therapy_off_edge_observe(&st, true, true);
  TEST_ASSERT_FALSE(therapy_off_edge_due(&st, T0));
}

// Seguir apagado vuelta tras vuelta no es un flanco nuevo.
void test_staying_off_after_publish_is_not_due_again(void) {
  therapy_off_edge_observe(&st, true, false);
  therapy_off_edge_observe(&st, false, false);
  therapy_off_edge_published(&st, T0);
  for (int i = 0; i < 20; i++) therapy_off_edge_observe(&st, false, false);
  TEST_ASSERT_FALSE(therapy_off_edge_due(&st, T0 + 10 * GAP));
}

// --- Pendiente hasta que sale ---------------------------------------------

// Sin conexion con ThingsBoard el envio no sale, pero el pendiente no se
// pierde: sale en cuanto se pueda publicar.
void test_pending_survives_until_published(void) {
  therapy_off_edge_observe(&st, true, false);
  therapy_off_edge_observe(&st, false, false);
  for (int i = 0; i < 50; i++) therapy_off_edge_observe(&st, false, false);
  TEST_ASSERT_TRUE(therapy_off_edge_due(&st, T0 + 60000u));
  therapy_off_edge_published(&st, T0 + 60000u);
  TEST_ASSERT_FALSE(therapy_off_edge_due(&st, T0 + 60000u));
}

// Una publicacion periodica que sale despues del flanco ya lleva el estado
// apagado: tambien despacha el pendiente.
void test_any_publish_clears_pending(void) {
  therapy_off_edge_observe(&st, false, true);
  therapy_off_edge_observe(&st, false, false);
  therapy_off_edge_published(&st, T0);
  TEST_ASSERT_FALSE(therapy_off_edge_due(&st, T0 + GAP));
}

// --- Separacion minima entre envios ---------------------------------------

// Si se acaba de publicar (p. ej. el periodico de 60 s justo antes del
// apagado), el forzado espera a la separacion minima, pero no se pierde.
void test_edge_right_after_a_publish_waits_min_gap(void) {
  therapy_off_edge_observe(&st, true, false);
  therapy_off_edge_published(&st, T0);
  therapy_off_edge_observe(&st, false, false);
  TEST_ASSERT_FALSE(therapy_off_edge_due(&st, T0 + GAP - 1));
  TEST_ASSERT_TRUE(therapy_off_edge_due(&st, T0 + GAP));
}

// Encender y apagar varias veces seguidas no satura el modem: como mucho un
// envio forzado por separacion minima.
void test_rapid_toggling_is_rate_limited(void) {
  therapy_off_edge_observe(&st, true, false);
  therapy_off_edge_observe(&st, false, false);
  TEST_ASSERT_TRUE(therapy_off_edge_due(&st, T0));
  therapy_off_edge_published(&st, T0);

  therapy_off_edge_observe(&st, true, false);
  therapy_off_edge_observe(&st, false, false);
  TEST_ASSERT_FALSE(therapy_off_edge_due(&st, T0 + 1000u));
  TEST_ASSERT_TRUE(therapy_off_edge_due(&st, T0 + GAP));
}

// Sin ninguna publicacion previa no hay nada que esperar, aunque millis()
// sea todavia menor que la separacion minima.
void test_no_previous_publish_does_not_delay(void) {
  therapy_off_edge_observe(&st, true, false);
  therapy_off_edge_observe(&st, false, false);
  TEST_ASSERT_TRUE(therapy_off_edge_due(&st, 5u));
}

// millis() de 32 bits da la vuelta a los ~49 dias: la resta sin signo tiene
// que seguir midiendo bien la separacion.
void test_min_gap_survives_millis_wraparound(void) {
  const uint32_t nearWrap = 0xFFFFFFFFu - 1000u;
  therapy_off_edge_observe(&st, true, false);
  therapy_off_edge_published(&st, nearWrap);
  therapy_off_edge_observe(&st, false, false);
  TEST_ASSERT_FALSE(therapy_off_edge_due(&st, nearWrap + 2000u));
  TEST_ASSERT_TRUE(therapy_off_edge_due(&st, nearWrap + GAP));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_first_observation_is_not_an_edge);
  RUN_TEST(test_first_observation_with_everything_on_is_not_an_edge);
  RUN_TEST(test_phototherapy_turning_off_is_due);
  RUN_TEST(test_control_turning_off_is_due);
  RUN_TEST(test_one_turning_off_while_other_stays_on_is_due);
  RUN_TEST(test_turning_on_is_not_due);
  RUN_TEST(test_staying_off_after_publish_is_not_due_again);
  RUN_TEST(test_pending_survives_until_published);
  RUN_TEST(test_any_publish_clears_pending);
  RUN_TEST(test_edge_right_after_a_publish_waits_min_gap);
  RUN_TEST(test_rapid_toggling_is_rate_limited);
  RUN_TEST(test_no_previous_publish_does_not_delay);
  RUN_TEST(test_min_gap_survives_millis_wraparound);
  return UNITY_END();
}

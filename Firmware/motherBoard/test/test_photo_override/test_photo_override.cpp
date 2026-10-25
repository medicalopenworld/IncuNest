// Regresion del encendido de fototerapia desde el modo depuracion.
//
// Lo que se protege es que una prueba de banco no se cuele en el estado real:
// que al soltarla la lampara vuelva a como estaba aunque el display haya
// adoptado el ON, y que nada de la prueba se guarde en NVS (una caida lo
// reanudaria). Ver modules/control/photo_override.h.

#include <unity.h>

#include "modules/control/photo_override.h"

static PhotoOverride o;
static uint32_t now;

void setUp(void) {
  photo_override_init(&o);
  now = 100000;  // lejos de 0
}
void tearDown(void) {}

// --- sin encendido de depuracion, manda el display y se guarda ---

void test_idle_follows_display_and_persists(void) {
  TEST_ASSERT_FALSE(photo_override_active(&o));
  TEST_ASSERT_TRUE(photo_override_effective(&o, true, now));
  TEST_ASSERT_FALSE(photo_override_effective(&o, false, now));
  TEST_ASSERT_TRUE(photo_override_may_persist(&o, now));
}

// --- mientras esta activo ---

void test_active_forces_on_even_if_display_says_off(void) {
  photo_override_start(&o, false, now);
  TEST_ASSERT_TRUE(photo_override_active(&o));
  TEST_ASSERT_TRUE(photo_override_effective(&o, false, now));
}

void test_active_never_persists(void) {
  photo_override_start(&o, false, now);
  // El display ha adoptado el ON y lo devuelve: ese 1 NO se puede guardar.
  TEST_ASSERT_FALSE(photo_override_may_persist(&o, now));
  photo_override_effective(&o, true, now);
  TEST_ASSERT_FALSE(photo_override_may_persist(&o, now));
}

// --- EL FALLO QUE MOTIVA TODO: al soltar, el display sigue diciendo 1 ---

void test_release_ignores_stale_on_from_display(void) {
  photo_override_start(&o, false, now);
  photo_override_release(&o, now);
  TEST_ASSERT_FALSE(photo_override_active(&o));
  // El display adopto el ON durante la prueba y aun lo devuelve.
  now += 1000;
  TEST_ASSERT_FALSE(photo_override_effective(&o, true, now));
  now += 3000;
  TEST_ASSERT_FALSE(photo_override_effective(&o, true, now));
}

void test_release_does_not_persist_during_grace(void) {
  photo_override_start(&o, false, now);
  photo_override_release(&o, now);
  now += 2000;
  TEST_ASSERT_FALSE(photo_override_may_persist(&o, now));
}

// --- liberacion CONFIRMADA (no una ventana fija) ---

// El fallo que motiva el cambio: la tarea Comm del display se atasca y sigue
// devolviendo el ON adoptado MUCHO despues de los 5 s de la ventana antigua.
// Antes main.cpp aplicaba y guardaba en NVS ese 1.
void test_display_still_returning_on_after_5s_is_ignored(void) {
  photo_override_start(&o, false, now);
  photo_override_release(&o, now);
  const uint32_t offsets[] = {5001u, 8000u, 20000u, 59000u};
  for (unsigned i = 0; i < 4; ++i) {
    now = 100000 + offsets[i];
    TEST_ASSERT_FALSE(photo_override_effective(&o, true, now));
    TEST_ASSERT_FALSE(photo_override_may_persist(&o, now));
  }
}

// Tampoco al vencer el plazo largo: nunca se adopta un 1 sin confirmar.
void test_backstop_never_adopts_unconfirmed_on(void) {
  photo_override_start(&o, false, now);
  photo_override_release(&o, now);
  now += PHOTO_OVERRIDE_RELEASE_BACKSTOP_MS + 1u;
  TEST_ASSERT_FALSE(photo_override_effective(&o, true, now));
  TEST_ASSERT_FALSE(photo_override_may_persist(&o, now));
  now += 3600000u;
  TEST_ASSERT_FALSE(photo_override_effective(&o, true, now));
  TEST_ASSERT_FALSE(photo_override_may_persist(&o, now));
}

// El aviso de plazo vencido sale una sola vez y no antes.
void test_overdue_is_reported_once_and_not_before(void) {
  photo_override_start(&o, false, now);
  photo_override_release(&o, now);
  now += PHOTO_OVERRIDE_RELEASE_BACKSTOP_MS - 1u;
  TEST_ASSERT_FALSE(photo_override_unconfirmed_overdue(&o, now));
  now += 2u;
  TEST_ASSERT_TRUE(photo_override_unconfirmed_overdue(&o, now));
  TEST_ASSERT_FALSE(photo_override_unconfirmed_overdue(&o, now + 1000u));
}

// Una confirmacion a los 8 s: el display ya devuelve el valor real, asi que
// a partir de ahi manda el display y se puede guardar.
void test_confirmation_at_8s_hands_control_back(void) {
  photo_override_start(&o, false, now);
  photo_override_release(&o, now);
  now += 3000u;
  TEST_ASSERT_FALSE(photo_override_effective(&o, true, now));  // rancio
  now += 5000u;                                                // t = 8 s
  TEST_ASSERT_FALSE(photo_override_effective(&o, false, now)); // confirma
  TEST_ASSERT_TRUE(photo_override_may_persist(&o, now));
  // Ya confirmado, un ON del display es una orden real del operario.
  now += 1000u;
  TEST_ASSERT_TRUE(photo_override_effective(&o, true, now));
  TEST_ASSERT_TRUE(photo_override_may_persist(&o, now));
}

// Si la prueba arranco con la lampara real encendida, "confirmar" es que el
// display devuelva 1.
void test_confirmation_when_real_was_on(void) {
  photo_override_start(&o, true, now);
  photo_override_release(&o, now);
  now += 3000u;
  TEST_ASSERT_TRUE(photo_override_effective(&o, true, now));
  TEST_ASSERT_TRUE(photo_override_may_persist(&o, now));
}

// Una trama del display ya en vuelo de ANTES de adoptar el ON tambien lleva
// el valor real: no es una confirmacion si llega recien soltado.
void test_early_echo_of_real_value_is_not_a_confirmation(void) {
  photo_override_start(&o, false, now);
  photo_override_release(&o, now);
  now += 500u;
  TEST_ASSERT_FALSE(photo_override_effective(&o, false, now));
  TEST_ASSERT_FALSE(photo_override_may_persist(&o, now));
  // Y el ON que el display adopta despues sigue ignorado.
  now += 1000u;
  TEST_ASSERT_FALSE(photo_override_effective(&o, true, now));
  TEST_ASSERT_FALSE(photo_override_may_persist(&o, now));
}

// --- volver a como estaba ANTES, no siempre a apagado ---

void test_release_restores_real_on_when_it_was_on(void) {
  // El operario tenia fototerapia puesta cuando empezo la prueba.
  photo_override_start(&o, true, now);
  photo_override_release(&o, now);
  now += 1000;
  TEST_ASSERT_TRUE(photo_override_effective(&o, false, now));
}

// --- una segunda orden de encendido no pisa el estado real bueno ---

void test_start_twice_keeps_first_real_state(void) {
  photo_override_start(&o, false, now);
  // Para entonces in3.phototherapy ya vale 1 por la propia prueba.
  photo_override_start(&o, true, now);
  photo_override_release(&o, now);
  now += 1000;
  TEST_ASSERT_FALSE(photo_override_effective(&o, true, now));
}

void test_release_when_idle_is_harmless(void) {
  photo_override_release(&o, now);
  TEST_ASSERT_TRUE(photo_override_effective(&o, true, now));
  TEST_ASSERT_TRUE(photo_override_may_persist(&o, now));
}

// --- volver a encender durante la gracia ---

void test_restart_during_grace_forces_on_again(void) {
  photo_override_start(&o, false, now);
  photo_override_release(&o, now);
  now += 1000;
  photo_override_start(&o, true, now);  // el true es el valor contaminado
  TEST_ASSERT_TRUE(photo_override_active(&o));
  TEST_ASSERT_TRUE(photo_override_effective(&o, false, now));
  photo_override_release(&o, now);
  now += 1000;
  // Tiene que volver al real de la PRIMERA prueba (apagado), no al contaminado.
  TEST_ASSERT_FALSE(photo_override_effective(&o, true, now));
}

// --- desbordamiento de millis() ---

void test_grace_survives_millis_wraparound(void) {
  now = 0xFFFFFFFFu - 1000u;
  photo_override_start(&o, false, now);
  photo_override_release(&o, now);
  now += 2000;  // ha dado la vuelta
  TEST_ASSERT_FALSE(photo_override_effective(&o, true, now));
  now += PHOTO_OVERRIDE_CONFIRM_MIN_MS;
  // Confirma el display (devuelve el valor real) y despues manda el display.
  TEST_ASSERT_FALSE(photo_override_effective(&o, false, now));
  TEST_ASSERT_TRUE(photo_override_effective(&o, true, now));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_follows_display_and_persists);
  RUN_TEST(test_active_forces_on_even_if_display_says_off);
  RUN_TEST(test_active_never_persists);
  RUN_TEST(test_release_ignores_stale_on_from_display);
  RUN_TEST(test_release_does_not_persist_during_grace);
  RUN_TEST(test_display_still_returning_on_after_5s_is_ignored);
  RUN_TEST(test_backstop_never_adopts_unconfirmed_on);
  RUN_TEST(test_overdue_is_reported_once_and_not_before);
  RUN_TEST(test_confirmation_at_8s_hands_control_back);
  RUN_TEST(test_confirmation_when_real_was_on);
  RUN_TEST(test_early_echo_of_real_value_is_not_a_confirmation);
  RUN_TEST(test_release_restores_real_on_when_it_was_on);
  RUN_TEST(test_start_twice_keeps_first_real_state);
  RUN_TEST(test_release_when_idle_is_harmless);
  RUN_TEST(test_restart_during_grace_forces_on_again);
  RUN_TEST(test_grace_survives_millis_wraparound);
  return UNITY_END();
}

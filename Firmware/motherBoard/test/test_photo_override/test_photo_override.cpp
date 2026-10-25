// Regresion del encendido de fototerapia desde el modo depuracion.
//
// Lo que se protege es que una prueba de banco no se cuele en el estado real:
// que el display nunca adopte el ON forzado (el campo `photo` de CTRL,STATE
// emite su propia orden mientras dura), que al soltar la lampara vuelva EN EL
// ACTO a la ultima orden del operador aunque no llegue ninguna trama, y que
// nada de la prueba se guarde en NVS. Ver modules/control/photo_override.h.

#include <unity.h>

#include "modules/control/photo_override.h"

static PhotoOverride o;

void setUp(void) { photo_override_init(&o); }
void tearDown(void) {}

// Suelta y devuelve lo que hay que aplicar a la lampara (false si no habia
// nada pendiente: el valor devuelto en ese caso no se mira).
static bool release_and_take(bool *applied) {
  photo_override_release(&o);
  return photo_override_take_release(&o, applied);
}

// --- sin prueba, manda el display y se guarda ---

void test_idle_follows_display_and_persists(void) {
  TEST_ASSERT_FALSE(photo_override_active(&o));
  TEST_ASSERT_TRUE(photo_override_effective(&o, true));
  TEST_ASSERT_FALSE(photo_override_effective(&o, false));
  TEST_ASSERT_TRUE(photo_override_may_persist(&o));
}

void test_idle_state_field_is_the_real_state(void) {
  TEST_ASSERT_TRUE(photo_override_state_field(&o, true));
  TEST_ASSERT_FALSE(photo_override_state_field(&o, false));
}

void test_idle_forces_nothing(void) {
  bool v = true;
  TEST_ASSERT_FALSE(photo_override_forced(&o, &v));
  TEST_ASSERT_FALSE(photo_override_take_release(&o, &v));
}

// --- mientras esta activa ---

void test_active_forces_on_even_if_display_says_off(void) {
  photo_override_start(&o, false);
  TEST_ASSERT_TRUE(photo_override_active(&o));
  TEST_ASSERT_TRUE(photo_override_effective(&o, false));
  bool v = false;
  TEST_ASSERT_TRUE(photo_override_forced(&o, &v));
  TEST_ASSERT_TRUE(v);
}

void test_active_never_persists(void) {
  photo_override_start(&o, false);
  TEST_ASSERT_FALSE(photo_override_may_persist(&o));
  photo_override_effective(&o, true);
  TEST_ASSERT_FALSE(photo_override_may_persist(&o));
}

// --- EL CAMBIO: el campo 8 emite la orden del display, no el ON forzado ---

// Con real OFF, la prueba fuerza la lampara pero el campo `photo` sigue
// diciendo 0: el display no adopta nada y no hay ningun 1 que neutralizar.
void test_state_field_during_test_is_the_display_order_not_the_forced_on(void) {
  photo_override_start(&o, false);
  // La lampara esta REALMENTE encendida (applied=true) por la prueba...
  TEST_ASSERT_FALSE(photo_override_state_field(&o, true));
  // ... y si el operador ordena ON desde la pantalla, el campo lo refleja.
  photo_override_effective(&o, true);
  TEST_ASSERT_TRUE(photo_override_state_field(&o, true));
  // Y si lo apaga desde la pantalla, tambien.
  photo_override_effective(&o, false);
  TEST_ASSERT_FALSE(photo_override_state_field(&o, true));
}

// Con real ON, durante la prueba el campo sigue diciendo 1.
void test_state_field_during_test_with_real_on(void) {
  photo_override_start(&o, true);
  TEST_ASSERT_TRUE(photo_override_state_field(&o, true));
}

// Soltada la prueba, el campo vuelve a ser el estado aplicado.
void test_state_field_after_release_is_the_applied_state(void) {
  photo_override_start(&o, false);
  photo_override_release(&o);
  TEST_ASSERT_FALSE(photo_override_state_field(&o, false));
  TEST_ASSERT_TRUE(photo_override_state_field(&o, true));
}

// --- soltar: vuelve a la ultima orden del display, EN EL ACTO ---

// real OFF, prueba, soltar sin ninguna trama (display colgado) -> OFF.
void test_real_off_release_without_frames_turns_off(void) {
  bool applied = true;
  photo_override_start(&o, false);
  TEST_ASSERT_TRUE(release_and_take(&applied));
  TEST_ASSERT_FALSE(applied);
  // Sin confirmacion ni plazos: ya no fuerza nada y se puede guardar.
  bool v = true;
  TEST_ASSERT_FALSE(photo_override_forced(&o, &v));
  TEST_ASSERT_TRUE(photo_override_may_persist(&o));
}

// real ON, prueba, soltar sin tramas -> sigue ON (era lo que habia).
void test_real_on_release_without_frames_stays_on(void) {
  bool applied = false;
  photo_override_start(&o, true);
  TEST_ASSERT_TRUE(release_and_take(&applied));
  TEST_ASSERT_TRUE(applied);
}

// real ON, prueba, vence el temporizador de fototerapia (orden de apagado de
// la placa), soltar -> OFF. Antes el ON se reencendia y se guardaba como
// sesion continua.
void test_real_on_timer_expires_during_test_release_turns_off(void) {
  bool applied = true;
  photo_override_start(&o, true);
  photo_override_note_order(&o, false);  // caduca el temporizador
  // Durante la prueba la lampara sigue forzada a ON...
  bool v = false;
  TEST_ASSERT_TRUE(photo_override_forced(&o, &v));
  TEST_ASSERT_TRUE(v);
  // ... y al soltar queda OFF; nada ON que guardar.
  TEST_ASSERT_TRUE(release_and_take(&applied));
  TEST_ASSERT_FALSE(applied);
  TEST_ASSERT_FALSE(photo_override_state_field(&o, applied));
}

// real ON, prueba, el operador manda OFF durante la prueba, soltar -> OFF.
// Antes ese OFF se perdia.
void test_real_on_operator_orders_off_during_test_release_turns_off(void) {
  bool applied = true;
  photo_override_start(&o, true);
  TEST_ASSERT_TRUE(photo_override_effective(&o, false));  // sigue forzada ON
  TEST_ASSERT_TRUE(release_and_take(&applied));
  TEST_ASSERT_FALSE(applied);
}

// real OFF, el operador ordena ON durante la prueba, soltar -> ON: la orden
// del operador manda.
void test_operator_orders_on_during_test_release_turns_on(void) {
  bool applied = false;
  photo_override_start(&o, false);
  photo_override_effective(&o, true);
  TEST_ASSERT_TRUE(release_and_take(&applied));
  TEST_ASSERT_TRUE(applied);
}

// La ultima orden gana.
void test_last_display_order_wins(void) {
  bool applied = true;
  photo_override_start(&o, false);
  photo_override_effective(&o, true);
  photo_override_effective(&o, false);
  photo_override_effective(&o, true);
  photo_override_effective(&o, false);
  TEST_ASSERT_TRUE(release_and_take(&applied));
  TEST_ASSERT_FALSE(applied);
}

// Lo pendiente se entrega UNA sola vez.
void test_release_is_taken_once(void) {
  bool applied = false;
  photo_override_start(&o, true);
  TEST_ASSERT_TRUE(release_and_take(&applied));
  TEST_ASSERT_FALSE(photo_override_take_release(&o, &applied));
}

// --- casos de borde ---

void test_release_when_idle_is_harmless(void) {
  bool v = true;
  photo_override_release(&o);
  TEST_ASSERT_FALSE(photo_override_take_release(&o, &v));
  TEST_ASSERT_TRUE(photo_override_effective(&o, true));
  TEST_ASSERT_TRUE(photo_override_may_persist(&o));
}

// Una orden fuera de la prueba no se registra.
void test_note_order_when_idle_is_ignored(void) {
  bool applied = true;
  photo_override_note_order(&o, true);
  photo_override_start(&o, false);
  TEST_ASSERT_TRUE(release_and_take(&applied));
  TEST_ASSERT_FALSE(applied);
}

// Una segunda orden de encendido no pisa las ordenes del operador: el valor
// "actual" que llega ya esta contaminado por la prueba.
void test_start_twice_keeps_the_operator_order(void) {
  bool applied = true;
  photo_override_start(&o, false);
  photo_override_start(&o, true);  // el true es el valor contaminado
  TEST_ASSERT_TRUE(release_and_take(&applied));
  TEST_ASSERT_FALSE(applied);
}

// Volver a encender antes de aplicar lo pendiente cancela la liberacion.
void test_restart_cancels_a_pending_release(void) {
  bool applied = false;
  photo_override_start(&o, false);
  photo_override_release(&o);
  photo_override_start(&o, true);
  TEST_ASSERT_FALSE(photo_override_take_release(&o, &applied));
  TEST_ASSERT_TRUE(photo_override_active(&o));
  TEST_ASSERT_TRUE(photo_override_effective(&o, false));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_follows_display_and_persists);
  RUN_TEST(test_idle_state_field_is_the_real_state);
  RUN_TEST(test_idle_forces_nothing);
  RUN_TEST(test_active_forces_on_even_if_display_says_off);
  RUN_TEST(test_active_never_persists);
  RUN_TEST(test_state_field_during_test_is_the_display_order_not_the_forced_on);
  RUN_TEST(test_state_field_during_test_with_real_on);
  RUN_TEST(test_state_field_after_release_is_the_applied_state);
  RUN_TEST(test_real_off_release_without_frames_turns_off);
  RUN_TEST(test_real_on_release_without_frames_stays_on);
  RUN_TEST(test_real_on_timer_expires_during_test_release_turns_off);
  RUN_TEST(test_real_on_operator_orders_off_during_test_release_turns_off);
  RUN_TEST(test_operator_orders_on_during_test_release_turns_on);
  RUN_TEST(test_last_display_order_wins);
  RUN_TEST(test_release_is_taken_once);
  RUN_TEST(test_release_when_idle_is_harmless);
  RUN_TEST(test_note_order_when_idle_is_ignored);
  RUN_TEST(test_start_twice_keeps_the_operator_order);
  RUN_TEST(test_restart_cancels_a_pending_release);
  return UNITY_END();
}

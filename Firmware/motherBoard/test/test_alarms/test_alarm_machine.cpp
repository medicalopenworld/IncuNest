#include <unity.h>
#include "modules/control/alarm_machine.h"
#include "modules/control/alarm_window.h"

void setUp(void) { alarm_machine_init(); }
void tearDown(void) {}

void test_starts_inactive(void) {
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  TEST_ASSERT_EQUAL_UINT32(0u, alarm_machine_bitmask());
}

// Sin retardo de anuncio configurado, una condicion presente se anuncia ya.
void test_condition_becomes_active(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_FAN_FAILURE));
}

// Non-latching: al irse la condicion, la alarma se va sola.
void test_non_latching_clears_itself(void) {
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, false, 2000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  TEST_ASSERT_EQUAL_UINT32(0u, alarm_machine_bitmask());
}

// Repetir la misma condicion no debe reiniciar nada ni duplicar estado.
void test_repeated_condition_is_idempotent(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1000);
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1500);
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 2000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
}

// El corte de calefactor lo dicta la maquina, no el llamante.
void test_heater_cut_follows_policy(void) {
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 1000);
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1000);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// 201.12.3.101: una salida de aire obstruida debe cortar el calefactor.
void test_blocked_outlet_cuts_the_heater(void) {
  alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, true, 1000);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// 201.15.4.2.1 dd)/ee): por el lado frio el calefactor sigue encendido.
void test_cold_deviation_does_not_cut_the_heater(void) {
  alarm_machine_condition(ALARM_AIR_TEMP_DEVIATION_LOW, true, 1000);
  alarm_machine_condition(ALARM_SKIN_TEMP_DEVIATION_LOW, true, 1000);
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
}

// Con retardo configurado, la condicion se registra pero no se anuncia.
void test_delay_holds_in_pending(void) {
  alarm_machine_set_announce_delay(ALARM_HUMIDITY_DEVIATION, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_PENDING,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  // Pero la condicion ya cuenta como senalizada: el operador debe poder verla.
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_HUMIDITY_DEVIATION));
}

// Al expirar el retardo con la condicion viva, se anuncia. Que el retardo
// cancelase el aviso para siempre seria el fallo que tenia el codigo anterior.
void test_delay_expiry_announces(void) {
  alarm_machine_set_announce_delay(ALARM_HUMIDITY_DEVIATION, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  alarm_machine_tick(999);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_PENDING,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  alarm_machine_tick(1000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
}

// Si la condicion se va durante el retardo, no llega a anunciarse nunca.
void test_condition_gone_during_delay_never_announces(void) {
  alarm_machine_set_announce_delay(ALARM_HUMIDITY_DEVIATION, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, false, 500);
  alarm_machine_tick(2000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
}

// Un corte termico no admite retardo aunque se le configure uno: 201.15.4.2.1
// exige aviso en el instante en que se dispara.
void test_thermal_cutout_ignores_any_delay(void) {
  alarm_machine_set_announce_delay(ALARM_AIR_THERMAL_CUTOUT, 60000);
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_AIR_THERMAL_CUTOUT));
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
}

// Silenciar calla el audio pero la senal visual sigue (6.8.1: AUDIO PAUSED no
// puede inactivar la senal visual de 1 m).
void test_silence_stops_audio_but_not_the_visual_signal(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_silence(ALARM_FAN_FAILURE, 120000, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_SILENCED,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_FAN_FAILURE));
}

// Al expirar el silencio con la condicion viva, el audio vuelve
// (201.12.3.104: "deben reanudar automaticamente su funcion normal").
void test_silence_expiry_resumes_audio(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_silence(ALARM_FAN_FAILURE, 120000, 0);
  alarm_machine_tick(119999);
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  alarm_machine_tick(120000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
}

// El requisito nuclear de 6.8.1: silenciar una NO silencia a las otras.
void test_silencing_one_leaves_the_others_audible(void) {
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  alarm_machine_silence(ALARM_HUMIDITY_DEVIATION, 120000, 0);
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  // Llega un corte termico mientras la otra sigue silenciada.
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 1000);
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_SILENCED,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
}

// ACK inactiva el audio de forma indefinida, pero no la senal visual.
void test_ack_is_indefinite_and_keeps_the_visual_signal(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_ack(ALARM_FAN_FAILURE, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACKED,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  alarm_machine_tick(3600000);
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_FAN_FAILURE));
}

// Silenciar o aceptar no altera la proteccion del actuador.
void test_silencing_never_restores_the_heater(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_silence(ALARM_FAN_FAILURE, 120000, 0);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
  alarm_machine_ack(ALARM_FAN_FAILURE, 0);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// Silenciar una condicion que no se esta anunciando no debe hacer nada.
void test_silencing_an_inactive_condition_is_a_no_op(void) {
  alarm_machine_silence(ALARM_FAN_FAILURE, 120000, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
}

// El corte termico sigue senalizando aunque la temperatura vuelva a rango.
void test_thermal_cutout_survives_the_condition_clearing(void) {
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 0);
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, false, 5000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_AIR_THERMAL_CUTOUT));
  TEST_ASSERT_TRUE(alarm_machine_is_latched(ALARM_AIR_THERMAL_CUTOUT));
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_AIR_THERMAL_CUTOUT));
}

// Solo el reset manual la limpia, y solo si la condicion ya no esta presente.
void test_manual_reset_clears_a_latched_alarm(void) {
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 0);
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, false, 5000);
  TEST_ASSERT_TRUE(alarm_machine_reset(ALARM_AIR_THERMAL_CUTOUT, 6000));
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_AIR_THERMAL_CUTOUT));
}

// Resetear con la camara todavia caliente no puede apagar la alarma: seria
// devolver el calefactor a un estado peligroso por pulsar un boton.
void test_reset_is_refused_while_the_condition_persists(void) {
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 0);
  TEST_ASSERT_FALSE(alarm_machine_reset(ALARM_AIR_THERMAL_CUTOUT, 1000));
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_AIR_THERMAL_CUTOUT));
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// Mientras esta latcheada sin condicion, el calefactor puede volver: la norma
// solo exige mantener la ALARMA, no el corte, una vez bajada la temperatura.
void test_latched_without_condition_releases_the_heater(void) {
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 0);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, false, 5000);
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
}

// Una non-latching no necesita reset y lo rechaza.
void test_reset_on_a_non_latching_alarm_is_refused(void) {
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  TEST_ASSERT_FALSE(alarm_machine_reset(ALARM_HUMIDITY_DEVIATION, 1000));
}

// La prioridad activa es la mas alta de las que se estan anunciando.
void test_top_priority_is_the_highest_signalling(void) {
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_LOW, alarm_machine_top_priority());
  alarm_machine_condition(ALARM_HEATER_FAULT, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_MEDIUM, alarm_machine_top_priority());
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_machine_top_priority());
}

// Una condicion PENDING no eleva la prioridad activa: todavia no se anuncia.
void test_pending_does_not_raise_top_priority(void) {
  alarm_machine_set_announce_delay(ALARM_HEATER_FAULT, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  alarm_machine_condition(ALARM_HEATER_FAULT, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_LOW, alarm_machine_top_priority());
}

// C-2 (revision tarea 11): silenciar una ALTA no puede prestarle su prioridad
// al audio de una BAJA distinta que sigue activa. La senal VISUAL si debe
// seguir mostrando la ALTA (top_priority), pero el zumbador tiene que sonar
// como la BAJA que realmente exige audio.
void test_audible_priority_ignores_a_silenced_high_priority_alarm(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_silence(ALARM_FAN_FAILURE, 120000, 0);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 1000);
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_LOW, alarm_machine_audible_priority());
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_machine_top_priority());
}

// Una ALTA que si esta ACTIVE (exigiendo audio de verdad) si eleva la
// prioridad audible por encima de una BAJA tambien activa.
void test_audible_priority_reflects_an_active_high_priority_alarm(void) {
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_LOW, alarm_machine_audible_priority());
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1000);
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_machine_audible_priority());
}

// Mismo criterio que alarm_machine_audio_required(): la prioridad audible
// sigue reflejando la ALTA mientras dure su rafaga minima (6.10), aunque la
// condicion ya se haya retirado y el estado haya vuelto a INACTIVE.
void test_audible_priority_holds_through_the_minimum_burst_window(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_condition(ALARM_FAN_FAILURE, false, 10);
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_machine_audible_priority());
}

void test_no_alarms_means_nothing_signalling(void) {
  TEST_ASSERT_FALSE(alarm_machine_any_signalling());
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  TEST_ASSERT_TRUE(alarm_machine_any_signalling());
  alarm_machine_condition(ALARM_FAN_FAILURE, false, 1000);
  TEST_ASSERT_FALSE(alarm_machine_any_signalling());
}

// 6.10: una condicion que dura menos que su rafaga minima sigue exigiendo
// audio hasta completarla.
void test_short_condition_still_completes_its_burst(void) {
  alarm_machine_condition(ALARM_HEATER_FAULT, true, 0);
  alarm_machine_condition(ALARM_HEATER_FAULT, false, 10);
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
  alarm_machine_tick(ALARM_MIN_BURST_MS_MEDIUM - 1);
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
  alarm_machine_tick(ALARM_MIN_BURST_MS_MEDIUM);
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
}

// 6.10 exime de completar la rafaga minima cuando el OPERADOR ha inactivado
// la senal ("unless inactivated by the OPERATOR"): silenciar cancela la
// rafaga pendiente, si no el zumbador resucitaria tras retirarse la condicion
// para una alarma que el operador ya habia silenciado.
void test_silencing_cancels_the_minimum_burst_hold(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_silence(ALARM_FAN_FAILURE, 100, 50);
  alarm_machine_condition(ALARM_FAN_FAILURE, false, 80);
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
}

// Mismo requisito de 6.10 para ACK: aceptar tambien es una inactivacion del
// OPERADOR y cancela la rafaga pendiente.
void test_ack_cancels_the_minimum_burst_hold(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_ack(ALARM_FAN_FAILURE, 50);
  alarm_machine_condition(ALARM_FAN_FAILURE, false, 80);
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
}

// --- Invariantes de los que depende la integracion de security.cpp ---

// C-1: la ventana de estabilizacion se aplica como retardo de ANUNCIO, nunca
// reteniendo la condicion. 201.12.3.104 permite retrasar el aviso mientras la
// incubadora calienta desde frio; no permite retrasar el corte de calefactor,
// que es ESSENTIAL PERFORMANCE. Con la condicion en PENDING, heater_must_cut()
// tiene que decir true YA: mira la condicion fisica, no el estado de senal.
void test_pending_still_cuts_the_heater(void) {
  alarm_machine_set_announce_delay(ALARM_AIR_TEMP_DEVIATION_HIGH, 30u * 60000u);
  alarm_machine_condition(ALARM_AIR_TEMP_DEVIATION_HIGH, true, 1000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_PENDING,
                        alarm_machine_state(ALARM_AIR_TEMP_DEVIATION_HIGH));
  TEST_ASSERT_TRUE(alarm_machine_audio_required() == false);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// El lado frio no corta ni siquiera anunciandose: el bebe se enfria si el
// calefactor se apaga por estar por debajo de la consigna.
void test_pending_cold_deviation_never_cuts_the_heater(void) {
  alarm_machine_set_announce_delay(ALARM_AIR_TEMP_DEVIATION_LOW, 30u * 60000u);
  alarm_machine_condition(ALARM_AIR_TEMP_DEVIATION_LOW, true, 1000);
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
  alarm_machine_tick(1000 + 30u * 60000u);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_AIR_TEMP_DEVIATION_LOW));
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
}

// Un retardo de anuncio puesto DESPUES de que la condicion ya esta presente no
// puede devolverla a PENDING ni retirar el corte: declareHotDeviation() lo fija
// solo en el flanco de subida, y este test fija esa garantia de la maquina.
void test_delay_set_while_active_does_not_unannounce(void) {
  alarm_machine_condition(ALARM_AIR_TEMP_DEVIATION_HIGH, true, 1000);
  alarm_machine_set_announce_delay(ALARM_AIR_TEMP_DEVIATION_HIGH, 30u * 60000u);
  alarm_machine_tick(2000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_AIR_TEMP_DEVIATION_HIGH));
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// C-2: la condicion sobrevive a que el detector deje de mirarla. Si
// checkAirBlockage() sale sin declarar false, el corte se queda puesto para
// siempre — y al no ser latching, el reset manual lo rechaza. Este test fija
// el porque de retirar la condicion en cada salida temprana.
void test_condition_persists_until_declared_false(void) {
  alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, true, 1000);
  for (uint32_t t = 2000; t < 100000; t += 1000) {
    alarm_machine_tick(t); // el detector ya no declara nada
  }
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
  TEST_ASSERT_FALSE(alarm_machine_reset(ALARM_AIR_OUTLET_BLOCKED, 100000));
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());

  alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, false, 101000);
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
  TEST_ASSERT_EQUAL_UINT32(0u, alarm_machine_bitmask());
}

// N-2: el retardo de anuncio aplaza el AUDIO, no la senal visual. Una
// condicion en PENDING entra igual en el bitmask, y de ahi salen el display y
// la telemetria a nube. Es la razon por la que el lado frio de la desviacion
// tiene que cerrarse con una puerta durante el calentamiento y no basta con
// retrasarle el anuncio: si no, cada arranque normal mostraria una alarma
// MEDIA durante toda la rampa.
void test_pending_is_visible_in_the_bitmask(void) {
  alarm_machine_set_announce_delay(ALARM_AIR_TEMP_DEVIATION_LOW, 30u * 60000u);
  alarm_machine_condition(ALARM_AIR_TEMP_DEVIATION_LOW, true, 1000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_PENDING,
                        alarm_machine_state(ALARM_AIR_TEMP_DEVIATION_LOW));
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  TEST_ASSERT_TRUE(alarm_machine_bitmask() &
                   (1u << ALARM_AIR_TEMP_DEVIATION_LOW));
  TEST_ASSERT_TRUE(alarm_machine_any_signalling());
}

// N-1: declarar false destruye la condicion sin dejar rastro, y nadie la
// recupera salvo un detector que vuelva a declararla. Por eso checkAirBlockage()
// no puede retirarla en sus salidas tempranas antes de haber medido en lazo
// cerrado: la unica declaracion viva en ese momento es la del autotest de
// arranque, que ya no volvera a producirse en toda la sesion.
void test_clearing_loses_the_condition_permanently(void) {
  alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, true, 1000);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());

  alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, false, 2000);
  for (uint32_t t = 3000; t < 60000; t += 1000) {
    alarm_machine_tick(t);
  }
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_AIR_OUTLET_BLOCKED));
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
  TEST_ASSERT_EQUAL_UINT32(0u, alarm_machine_bitmask());
}

// R-1: ninguna accion del operador libera el corte de calefactor de una
// condicion no-latching congelada. Silenciar y aceptar solo tocan el audio, y
// el reset manual la rechaza por no ser latching. La UNICA salida es que un
// detector declare false — de ahi que checkAirBlockage() tenga que poder
// hacerlo tambien cuando su medida es permanentemente inviable (PID del
// ventilador deshabilitado), y no solo cuando ya midio alguna vez.
void test_no_operator_action_frees_a_non_latching_condition(void) {
  alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, true, 1000);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());

  alarm_machine_silence(ALARM_AIR_OUTLET_BLOCKED, 120000, 2000);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());

  alarm_machine_ack(ALARM_AIR_OUTLET_BLOCKED, 3000);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());

  TEST_ASSERT_FALSE(alarm_machine_reset(ALARM_AIR_OUTLET_BLOCKED, 4000));
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());

  alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, false, 5000);
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
}

// --- Ventana de estabilizacion (R-2) ---

#define WINDOW_MS (30u * 60000u)

// Una activacion "en frio" (already_elapsed = 0) deja la ventana entera por
// delante, contada desde AHORA. Este es el caso que el offset viejo rompia:
// devolvia 0 fijo, es decir la ventana se contaba desde el origen del reloj.
void test_window_starts_counting_from_now(void) {
  const uint32_t now = 5u * 60u * 60000u; // equipo encendido hace 5 h
  const uint32_t start = alarm_window_start(now, 0u);
  TEST_ASSERT_EQUAL_UINT32(WINDOW_MS,
                           alarm_window_remaining_ms(start, WINDOW_MS, now));
  TEST_ASSERT_EQUAL_UINT32(
      1u, alarm_window_remaining_ms(start, WINDOW_MS, now + WINDOW_MS - 1u));
  TEST_ASSERT_EQUAL_UINT32(
      0u, alarm_window_remaining_ms(start, WINDOW_MS, now + WINDOW_MS));
}

// La regresion concreta: iniciar la terapia mas de una ventana despues de
// encender el equipo — preparar la incubadora antes de empezar — no puede
// dejar la ventana nacida ya cerrada.
void test_therapy_started_late_still_gets_a_full_window(void) {
  const uint32_t now = 3u * 60u * 60000u; // 3 h de reloj
  const uint32_t start = alarm_window_start(now, 0u);
  TEST_ASSERT_FALSE(alarm_window_remaining_ms(start, WINDOW_MS, now) == 0u);
}

// RESTART_ALARM_GRACE_MINS = 0 se traduce en already_elapsed = ventana entera,
// y eso tiene que anular la ventana: un reinicio por WDT reanuda la terapia y
// no debe volver a esperar media hora.
void test_full_grace_opens_the_window_immediately(void) {
  const uint32_t now = 1000u;
  const uint32_t start = alarm_window_start(now, WINDOW_MS);
  TEST_ASSERT_EQUAL_UINT32(0u,
                           alarm_window_remaining_ms(start, WINDOW_MS, now));
}

// El reloj de millis() desborda a los ~49 dias en mitad de una ventana y la
// cuenta tiene que seguir siendo correcta.
void test_window_survives_the_millis_rollover(void) {
  const uint32_t now = 0xFFFFFFFFu - 60000u; // 1 min antes de desbordar
  const uint32_t start = alarm_window_start(now, 0u);
  TEST_ASSERT_EQUAL_UINT32(WINDOW_MS,
                           alarm_window_remaining_ms(start, WINDOW_MS, now));
  const uint32_t later = now + WINDOW_MS; // ya al otro lado del desbordamiento
  TEST_ASSERT_EQUAL_UINT32(0u,
                           alarm_window_remaining_ms(start, WINDOW_MS, later));
  TEST_ASSERT_EQUAL_UINT32(
      60000u, alarm_window_remaining_ms(start, WINDOW_MS, later - 60000u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_inactive);
  RUN_TEST(test_condition_becomes_active);
  RUN_TEST(test_non_latching_clears_itself);
  RUN_TEST(test_repeated_condition_is_idempotent);
  RUN_TEST(test_heater_cut_follows_policy);
  RUN_TEST(test_blocked_outlet_cuts_the_heater);
  RUN_TEST(test_cold_deviation_does_not_cut_the_heater);
  RUN_TEST(test_delay_holds_in_pending);
  RUN_TEST(test_delay_expiry_announces);
  RUN_TEST(test_condition_gone_during_delay_never_announces);
  RUN_TEST(test_thermal_cutout_ignores_any_delay);
  RUN_TEST(test_silence_stops_audio_but_not_the_visual_signal);
  RUN_TEST(test_silence_expiry_resumes_audio);
  RUN_TEST(test_silencing_one_leaves_the_others_audible);
  RUN_TEST(test_ack_is_indefinite_and_keeps_the_visual_signal);
  RUN_TEST(test_silencing_never_restores_the_heater);
  RUN_TEST(test_silencing_an_inactive_condition_is_a_no_op);
  RUN_TEST(test_thermal_cutout_survives_the_condition_clearing);
  RUN_TEST(test_manual_reset_clears_a_latched_alarm);
  RUN_TEST(test_reset_is_refused_while_the_condition_persists);
  RUN_TEST(test_latched_without_condition_releases_the_heater);
  RUN_TEST(test_reset_on_a_non_latching_alarm_is_refused);
  RUN_TEST(test_top_priority_is_the_highest_signalling);
  RUN_TEST(test_pending_does_not_raise_top_priority);
  RUN_TEST(test_audible_priority_ignores_a_silenced_high_priority_alarm);
  RUN_TEST(test_audible_priority_reflects_an_active_high_priority_alarm);
  RUN_TEST(test_audible_priority_holds_through_the_minimum_burst_window);
  RUN_TEST(test_no_alarms_means_nothing_signalling);
  RUN_TEST(test_short_condition_still_completes_its_burst);
  RUN_TEST(test_silencing_cancels_the_minimum_burst_hold);
  RUN_TEST(test_ack_cancels_the_minimum_burst_hold);
  RUN_TEST(test_pending_still_cuts_the_heater);
  RUN_TEST(test_pending_cold_deviation_never_cuts_the_heater);
  RUN_TEST(test_delay_set_while_active_does_not_unannounce);
  RUN_TEST(test_condition_persists_until_declared_false);
  RUN_TEST(test_pending_is_visible_in_the_bitmask);
  RUN_TEST(test_clearing_loses_the_condition_permanently);
  RUN_TEST(test_no_operator_action_frees_a_non_latching_condition);
  RUN_TEST(test_window_starts_counting_from_now);
  RUN_TEST(test_therapy_started_late_still_gets_a_full_window);
  RUN_TEST(test_full_grace_opens_the_window_immediately);
  RUN_TEST(test_window_survives_the_millis_rollover);
  return UNITY_END();
}

// Regresion de los dos fallos de ALARM_FAN_FAILURE que solo se vieron en banco
// (2026-09-11, unidad 353: sonda de aire desconectada -> ventilador oscilando,
// alarma de ventilador que ya no se retiraba).
//
// Ver modules/control/fan_guard.h para el relato completo.

#include <unity.h>

#include "modules/control/fan_guard.h"

static const FanGuardConfig CFG = {
    /*min_rpm*/ 3000.0,
    /*hysteresis_rpm*/ 300.0,
    /*spinup_grace_ms*/ 6000,
};

static FanGuard g;
static uint32_t now;

void setUp(void) {
  fan_guard_init(&g);
  now = 100000; // lejos de 0, para que un desbordamiento se note
}
void tearDown(void) {}

// Lleva la guarda a regimen: alimentada y con la gracia de arranque agotada.
static void settle_energised(double rpm) {
  fan_guard_update(&g, &CFG, true, true, rpm, now);
  now += CFG.spinup_grace_ms + 1;
}

// --- lo basico -------------------------------------------------------------

static void test_sin_tacometro_no_afirma_nada(void) {
  // Una unidad sin realimentacion de rpm no puede declarar NI que falle ni que
  // no: pisar con "ausente" seria afirmar algo que no se ha medido.
  TEST_ASSERT_EQUAL(FAN_GUARD_SILENT,
                    fan_guard_update(&g, &CFG, false, true, 0.0, now));
  now += 60000;
  TEST_ASSERT_EQUAL(FAN_GUARD_SILENT,
                    fan_guard_update(&g, &CFG, false, true, 0.0, now));
}

static void test_durante_el_arranque_no_declara_averia(void) {
  // Recien alimentado el ventilador esta parado de verdad; la averia seria un
  // falso positivo mecanico.
  TEST_ASSERT_EQUAL(FAN_GUARD_ABSENT,
                    fan_guard_update(&g, &CFG, true, false, 0.0, now));
  now += 10;
  TEST_ASSERT_EQUAL(FAN_GUARD_SILENT,
                    fan_guard_update(&g, &CFG, true, true, 0.0, now));
  now += CFG.spinup_grace_ms - 1;
  TEST_ASSERT_EQUAL(FAN_GUARD_SILENT,
                    fan_guard_update(&g, &CFG, true, true, 0.0, now));
  now += 2;
  TEST_ASSERT_EQUAL(FAN_GUARD_PRESENT,
                    fan_guard_update(&g, &CFG, true, true, 0.0, now));
}

static void test_girando_bien_no_hay_averia(void) {
  settle_energised(4000.0);
  TEST_ASSERT_EQUAL(FAN_GUARD_ABSENT,
                    fan_guard_update(&g, &CFG, true, true, 4000.0, now));
}

static void test_histeresis_pide_superar_el_umbral_mas_el_margen(void) {
  settle_energised(4000.0);
  // Cae por debajo: se declara.
  TEST_ASSERT_EQUAL(FAN_GUARD_PRESENT,
                    fan_guard_update(&g, &CFG, true, true, 2999.0, now));
  // Vuelve por encima del umbral pero sin el margen: sigue declarada.
  TEST_ASSERT_EQUAL(FAN_GUARD_PRESENT,
                    fan_guard_update(&g, &CFG, true, true, 3100.0, now));
  TEST_ASSERT_EQUAL(FAN_GUARD_PRESENT,
                    fan_guard_update(&g, &CFG, true, true, 3299.0, now));
  // Con el margen: se retira.
  TEST_ASSERT_EQUAL(FAN_GUARD_ABSENT,
                    fan_guard_update(&g, &CFG, true, true, 3301.0, now));
}

// --- FALLO 1: la alarma se realimentaba y no se podia retirar ---------------

static void test_la_averia_se_retira_cuando_el_ventilador_vuelve(void) {
  // Este es EL caso de banco. Antes del arreglo era inalcanzable: declarar la
  // alarma cortaba la alimentacion del ventilador, las rpm quedaban en 0 por
  // construccion y la condicion no podia volver a ser falsa nunca.
  settle_energised(4000.0);
  TEST_ASSERT_EQUAL(FAN_GUARD_PRESENT,
                    fan_guard_update(&g, &CFG, true, true, 1200.0, now));

  // El ventilador SIGUE ALIMENTADO con la alarma puesta (ese es el arreglo en
  // ongoingFanCriticalAlarm) y se recupera.
  now += 1000;
  TEST_ASSERT_EQUAL(FAN_GUARD_ABSENT,
                    fan_guard_update(&g, &CFG, true, true, 4000.0, now));
}

// --- FALLO 2: las salidas tempranas no retiraban la condicion ---------------

static void test_apagar_el_ventilador_retira_la_averia(void) {
  settle_energised(4000.0);
  TEST_ASSERT_EQUAL(FAN_GUARD_PRESENT,
                    fan_guard_update(&g, &CFG, true, true, 0.0, now));

  // Se apaga la actuacion. Sin la retirada, la maquina de alarmas conserva
  // `present` y el aviso se queda puesto para siempre.
  now += 10;
  TEST_ASSERT_EQUAL(FAN_GUARD_ABSENT,
                    fan_guard_update(&g, &CFG, true, false, 0.0, now));
}

static void test_tras_apagar_la_histeresis_vuelve_a_cero(void) {
  settle_energised(4000.0);
  fan_guard_update(&g, &CFG, true, true, 0.0, now); // averia declarada
  now += 10;
  fan_guard_update(&g, &CFG, true, false, 0.0, now); // apagado -> retirada

  // Al volver a arrancar, 3100 rpm es SUFICIENTE: el umbral relajado de la
  // histeresis (3300) pertenecia a la averia anterior y no debe sobrevivirla.
  now += 10;
  fan_guard_update(&g, &CFG, true, true, 0.0, now); // flanco: arranca la gracia
  now += CFG.spinup_grace_ms + 1;
  TEST_ASSERT_EQUAL(FAN_GUARD_ABSENT,
                    fan_guard_update(&g, &CFG, true, true, 3100.0, now));
}

// --- la gracia cuenta desde que se ALIMENTA, no desde que se ordena ---------

static void test_la_gracia_se_reinicia_al_recuperar_la_alimentacion(void) {
  // Escenario de subtension: la orden nunca se retira, pero la alimentacion si.
  // Midiendo la orden, al volver la tension la gracia llevaba agotada un buen
  // rato y el arranque mecanico contaba como averia.
  settle_energised(4000.0);
  TEST_ASSERT_EQUAL(FAN_GUARD_ABSENT,
                    fan_guard_update(&g, &CFG, true, true, 4000.0, now));

  // Corte de alimentacion (la orden sigue puesta, pero aqui llega ya resuelta).
  now += 100;
  TEST_ASSERT_EQUAL(FAN_GUARD_ABSENT,
                    fan_guard_update(&g, &CFG, true, false, 0.0, now));
  now += 30000; // la subtension dura

  // Vuelve la alimentacion: el ventilador arranca desde parado y tiene derecho
  // a su gracia otra vez.
  TEST_ASSERT_EQUAL(FAN_GUARD_SILENT,
                    fan_guard_update(&g, &CFG, true, true, 0.0, now));
  now += CFG.spinup_grace_ms - 1;
  TEST_ASSERT_EQUAL(FAN_GUARD_SILENT,
                    fan_guard_update(&g, &CFG, true, true, 500.0, now));
  now += 2;
  TEST_ASSERT_EQUAL(FAN_GUARD_ABSENT,
                    fan_guard_update(&g, &CFG, true, true, 4000.0, now));
}

// --- robustez --------------------------------------------------------------

static void test_el_desbordamiento_de_millis_no_dispara_la_averia(void) {
  // millis() desborda a los ~49.7 dias y una incubadora no se reinicia sola.
  // La resta sin signo tiene que seguir dando el intervalo correcto.
  fan_guard_init(&g);
  now = 0xFFFFFF00u;
  TEST_ASSERT_EQUAL(FAN_GUARD_SILENT,
                    fan_guard_update(&g, &CFG, true, true, 0.0, now));
  now += CFG.spinup_grace_ms - 1; // cruza el desbordamiento
  TEST_ASSERT_EQUAL(FAN_GUARD_SILENT,
                    fan_guard_update(&g, &CFG, true, true, 0.0, now));
  now += 2;
  TEST_ASSERT_EQUAL(FAN_GUARD_PRESENT,
                    fan_guard_update(&g, &CFG, true, true, 0.0, now));
}

static void test_puntero_nulo_no_revienta(void) {
  TEST_ASSERT_EQUAL(FAN_GUARD_SILENT,
                    fan_guard_update(NULL, &CFG, true, true, 0.0, now));
  TEST_ASSERT_EQUAL(FAN_GUARD_SILENT,
                    fan_guard_update(&g, NULL, true, true, 0.0, now));
  fan_guard_init(NULL); // no debe caerse
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_sin_tacometro_no_afirma_nada);
  RUN_TEST(test_durante_el_arranque_no_declara_averia);
  RUN_TEST(test_girando_bien_no_hay_averia);
  RUN_TEST(test_histeresis_pide_superar_el_umbral_mas_el_margen);
  RUN_TEST(test_la_averia_se_retira_cuando_el_ventilador_vuelve);
  RUN_TEST(test_apagar_el_ventilador_retira_la_averia);
  RUN_TEST(test_tras_apagar_la_histeresis_vuelve_a_cero);
  RUN_TEST(test_la_gracia_se_reinicia_al_recuperar_la_alimentacion);
  RUN_TEST(test_el_desbordamiento_de_millis_no_dispara_la_averia);
  RUN_TEST(test_puntero_nulo_no_revienta);
  return UNITY_END();
}

#include <unity.h>
#include <string.h>

#include "alarm_ids.h"
#include "alarm_policy.h"
#include "modules/control/alarm_history.h"

void setUp(void) { alarm_history_init(); }
void tearDown(void) {}

void test_starts_empty(void) {
  TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)alarm_history_count());
  TEST_ASSERT_NULL(alarm_history_get(0));
}

void test_records_a_raise(void) {
  alarm_history_record_raise(ALARM_FAN_FAILURE, ALARM_PRIORITY_HIGH, 1000, 0,
                             0);
  TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)alarm_history_count());
  const AlarmHistoryEntry *e = alarm_history_get(0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_UINT8(ALARM_FAN_FAILURE, e->id);
  TEST_ASSERT_EQUAL_UINT8(ALARM_PRIORITY_HIGH, e->priority);
  TEST_ASSERT_EQUAL_UINT32(1000u, e->raisedEpoch);
  TEST_ASSERT_EQUAL_UINT32(0u, e->clearedEpoch);
}

// El indice 0 es siempre la mas reciente: es lo que espera la pantalla, que
// lista de arriba abajo empezando por lo ultimo que paso.
void test_most_recent_comes_first(void) {
  alarm_history_record_raise(ALARM_FAN_FAILURE, ALARM_PRIORITY_HIGH, 1000, 0, 0);
  alarm_history_record_raise(ALARM_HUMIDITY_DEVIATION, ALARM_PRIORITY_LOW, 2000,
                             0, 0);
  TEST_ASSERT_EQUAL_UINT8(ALARM_HUMIDITY_DEVIATION, alarm_history_get(0)->id);
  TEST_ASSERT_EQUAL_UINT8(ALARM_FAN_FAILURE, alarm_history_get(1)->id);
}

// La resolucion se sella sobre la entrada existente y NO consume hueco: con
// diez huecos, registrar alta y baja por separado haria que una alarma que
// rebota cinco veces borrase todo lo anterior.
void test_clear_seals_the_entry_without_consuming_a_slot(void) {
  alarm_history_record_raise(ALARM_FAN_FAILURE, ALARM_PRIORITY_HIGH, 1000, 0, 0);
  TEST_ASSERT_TRUE(alarm_history_record_clear(ALARM_FAN_FAILURE, 1500));
  TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)alarm_history_count());
  TEST_ASSERT_EQUAL_UINT32(1500u, alarm_history_get(0)->clearedEpoch);
}

void test_clear_without_a_matching_raise_is_refused(void) {
  TEST_ASSERT_FALSE(alarm_history_record_clear(ALARM_FAN_FAILURE, 1500));
  alarm_history_record_raise(ALARM_FAN_FAILURE, ALARM_PRIORITY_HIGH, 1000, 0, 0);
  TEST_ASSERT_TRUE(alarm_history_record_clear(ALARM_FAN_FAILURE, 1500));
  // Ya sellada: una segunda resolucion no tiene nada que cerrar.
  TEST_ASSERT_FALSE(alarm_history_record_clear(ALARM_FAN_FAILURE, 1600));
}

// Con la misma condicion repetida, se cierra la que sigue abierta, no la
// primera que aparezca recorriendo.
void test_clear_seals_the_most_recent_open_entry(void) {
  alarm_history_record_raise(ALARM_FAN_FAILURE, ALARM_PRIORITY_HIGH, 1000, 0, 0);
  alarm_history_record_clear(ALARM_FAN_FAILURE, 1100);
  alarm_history_record_raise(ALARM_FAN_FAILURE, ALARM_PRIORITY_HIGH, 2000, 0, 0);
  TEST_ASSERT_TRUE(alarm_history_record_clear(ALARM_FAN_FAILURE, 2100));
  TEST_ASSERT_EQUAL_UINT32(2100u, alarm_history_get(0)->clearedEpoch);
  TEST_ASSERT_EQUAL_UINT32(1100u, alarm_history_get(1)->clearedEpoch);
}

// El anillo se llena y sobrescribe la mas antigua, sin desbordar.
void test_ring_wraps_at_capacity(void) {
  for (uint32_t i = 0; i < ALARM_HISTORY_CAPACITY + 5; ++i) {
    alarm_history_record_raise(ALARM_FAN_FAILURE, ALARM_PRIORITY_HIGH,
                               1000 + i, 0, 0);
  }
  TEST_ASSERT_EQUAL_UINT32((uint32_t)ALARM_HISTORY_CAPACITY,
                           (uint32_t)alarm_history_count());
  // La mas reciente es la ultima escrita; la mas antigua viva es la 5.
  TEST_ASSERT_EQUAL_UINT32(1000u + ALARM_HISTORY_CAPACITY + 4,
                           alarm_history_get(0)->raisedEpoch);
  TEST_ASSERT_EQUAL_UINT32(1005u,
                           alarm_history_get(ALARM_HISTORY_CAPACITY - 1)->raisedEpoch);
  TEST_ASSERT_NULL(alarm_history_get(ALARM_HISTORY_CAPACITY));
}

// Sin hora sincronizada el epoch es 0 y hay que conservarlo tal cual: la
// pantalla lo traduce a un guion en vez de a una fecha de 1970.
void test_zero_epoch_is_preserved(void) {
  alarm_history_record_raise(ALARM_AIR_SENSOR_FAULT, ALARM_PRIORITY_HIGH, 0, 0,
                             0);
  TEST_ASSERT_EQUAL_UINT32(0u, alarm_history_get(0)->raisedEpoch);
}

// 6.12.2 recomienda anotar el limite en vigor cuando es ajustable por el
// operador — los cortes termicos lo son — y el dato que disparo la condicion.
void test_limit_and_value_survive(void) {
  alarm_history_record_raise(ALARM_AIR_THERMAL_CUTOUT, ALARM_PRIORITY_HIGH,
                             1000, 3800, 3925);
  TEST_ASSERT_EQUAL_INT16(3800, alarm_history_get(0)->limitCenti);
  TEST_ASSERT_EQUAL_INT16(3925, alarm_history_get(0)->valueCenti);
}

void test_out_of_range_id_is_ignored(void) {
  alarm_history_record_raise((AlarmId)999, ALARM_PRIORITY_HIGH, 1000, 0, 0);
  alarm_history_record_raise(ALARM_NONE, ALARM_PRIORITY_HIGH, 1000, 0, 0);
  TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)alarm_history_count());
}

// Ida y vuelta por el blob que se persiste en NVS.
void test_serialize_round_trip(void) {
  alarm_history_record_raise(ALARM_FAN_FAILURE, ALARM_PRIORITY_HIGH, 1000, 10,
                             20);
  alarm_history_record_clear(ALARM_FAN_FAILURE, 1500);
  alarm_history_record_raise(ALARM_HUMIDITY_DEVIATION, ALARM_PRIORITY_LOW, 2000,
                             0, 0);

  uint8_t blob[512];
  const size_t n = alarm_history_serialize(blob, sizeof(blob));
  TEST_ASSERT_EQUAL_UINT32((uint32_t)alarm_history_blob_size(), (uint32_t)n);

  alarm_history_init();
  TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)alarm_history_count());

  TEST_ASSERT_TRUE(alarm_history_deserialize(blob, n));
  TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)alarm_history_count());
  TEST_ASSERT_EQUAL_UINT8(ALARM_HUMIDITY_DEVIATION, alarm_history_get(0)->id);
  TEST_ASSERT_EQUAL_UINT32(1500u, alarm_history_get(1)->clearedEpoch);
  TEST_ASSERT_EQUAL_INT16(20, alarm_history_get(1)->valueCenti);
}

void test_serialize_refuses_a_small_buffer(void) {
  uint8_t tiny[4];
  TEST_ASSERT_EQUAL_UINT32(0u,
                           (uint32_t)alarm_history_serialize(tiny, sizeof(tiny)));
}

// Un blob corrupto, truncado o de otra version se descarta entero y deja el
// historial vacio. Leerlo con el paso equivocado decodificaria basura como si
// fueran registros clinicos, que es peor que no tener historial.
void test_corrupt_blob_is_discarded(void) {
  uint8_t blob[512];
  alarm_history_record_raise(ALARM_FAN_FAILURE, ALARM_PRIORITY_HIGH, 1000, 0, 0);
  const size_t n = alarm_history_serialize(blob, sizeof(blob));

  blob[0] ^= 0xFF;  // magic roto
  TEST_ASSERT_FALSE(alarm_history_deserialize(blob, n));
  TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)alarm_history_count());

  blob[0] ^= 0xFF;
  TEST_ASSERT_FALSE(alarm_history_deserialize(blob, n - 1));  // truncado
  TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)alarm_history_count());

  TEST_ASSERT_FALSE(alarm_history_deserialize(NULL, n));
  TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)alarm_history_count());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_empty);
  RUN_TEST(test_records_a_raise);
  RUN_TEST(test_most_recent_comes_first);
  RUN_TEST(test_clear_seals_the_entry_without_consuming_a_slot);
  RUN_TEST(test_clear_without_a_matching_raise_is_refused);
  RUN_TEST(test_clear_seals_the_most_recent_open_entry);
  RUN_TEST(test_ring_wraps_at_capacity);
  RUN_TEST(test_zero_epoch_is_preserved);
  RUN_TEST(test_limit_and_value_survive);
  RUN_TEST(test_out_of_range_id_is_ignored);
  RUN_TEST(test_serialize_round_trip);
  RUN_TEST(test_serialize_refuses_a_small_buffer);
  RUN_TEST(test_corrupt_blob_is_discarded);
  return UNITY_END();
}

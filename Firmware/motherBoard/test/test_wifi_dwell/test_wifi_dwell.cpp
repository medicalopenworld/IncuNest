#include <string.h>
#include <unity.h>

#include "modules/util/wifi_dwell.h"

// Un dia UTC en segundos, para escribir los epochs de los tests sin cuentas.
#define DAY 86400u
// Un instante con reloj valido: 2021-01-01T00:00:00Z justo, el umbral.
#define T0 WIFI_DWELL_EPOCH_VALID

static WifiDwell st;

void setUp(void) { wifi_dwell_clear(&st); }
void tearDown(void) {}

// --- Estado inicial -------------------------------------------------------

void test_starts_empty(void) {
  TEST_ASSERT_EQUAL_STRING("", st.ssid);
  TEST_ASSERT_EQUAL_UINT32(0, st.firstEpoch);
  TEST_ASSERT_EQUAL_UINT16(0, st.days);
}

// --- Primera asociacion ---------------------------------------------------

void test_first_association_counts_one_day(void) {
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0));
  TEST_ASSERT_EQUAL_STRING("HospitalWiFi", st.ssid);
  TEST_ASSERT_EQUAL_UINT32(T0, st.firstEpoch);
  TEST_ASSERT_EQUAL_UINT16(1, st.days);
}

// --- Conteo por dias UTC distintos ---------------------------------------

// Reasociarse veinte veces el mismo dia no es informacion nueva: si esto
// devolviera true, cada parpadeo del enlace seria una escritura en NVS.
void test_same_day_reassociation_reports_no_change(void) {
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0));
  for (int i = 0; i < 20; i++) {
    TEST_ASSERT_FALSE(wifi_dwell_update(&st, "HospitalWiFi", T0 + 60u * i));
  }
  TEST_ASSERT_EQUAL_UINT16(1, st.days);
  TEST_ASSERT_EQUAL_UINT32(T0, st.firstEpoch);
}

void test_next_day_increments_once(void) {
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0));
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0 + DAY));
  TEST_ASSERT_EQUAL_UINT16(2, st.days);
  // El primer epoch no se mueve: es el ancla del span que publica el equipo.
  TEST_ASSERT_EQUAL_UINT32(T0, st.firstEpoch);
}

// El limite del dia es la medianoche UTC, no las 24 h desde la asociacion.
void test_day_boundary_is_utc_midnight(void) {
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0 + 23u * 3600u));
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0 + 25u * 3600u));
  TEST_ASSERT_EQUAL_UINT16(2, st.days);
}

// Un equipo desenchufado dos semanas no acumula esos dias, pero tampoco
// pierde los que ya tenia.
void test_non_contiguous_days_count(void) {
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0));
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0 + 9u * DAY));
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0 + 30u * DAY));
  TEST_ASSERT_EQUAL_UINT16(3, st.days);
}

// --- Cambio de red --------------------------------------------------------

void test_ssid_change_resets_the_count(void) {
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "TallerMOW", T0));
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "TallerMOW", T0 + DAY));
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "TallerMOW", T0 + 2u * DAY));
  TEST_ASSERT_EQUAL_UINT16(3, st.days);

  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0 + 3u * DAY));
  TEST_ASSERT_EQUAL_STRING("HospitalWiFi", st.ssid);
  TEST_ASSERT_EQUAL_UINT16(1, st.days);
  TEST_ASSERT_EQUAL_UINT32(T0 + 3u * DAY, st.firstEpoch);
}

// --- Reloj sin poner en hora ---------------------------------------------

// Sin reloj no se puede saber que dia es, asi que no se cuenta ninguno. El
// SNTP tarda segundos desde la asociacion, y un equipo sin internet ni NITZ
// puede no llegar nunca.
void test_unset_clock_counts_no_day(void) {
  // La primera pasada SI cambia el estado, porque apunta el SSID, y por eso
  // hay que persistirla. Lo que no puede hacer es contar un dia.
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", 0));
  TEST_ASSERT_EQUAL_STRING("HospitalWiFi", st.ssid);
  TEST_ASSERT_EQUAL_UINT16(0, st.days);
  TEST_ASSERT_EQUAL_UINT32(0, st.firstEpoch);
  // Y ya con el SSID apuntado, seguir sin reloj no cambia nada.
  TEST_ASSERT_FALSE(wifi_dwell_update(&st, "HospitalWiFi", 0));
  TEST_ASSERT_EQUAL_UINT16(0, st.days);
}

void test_epoch_below_threshold_counts_no_day(void) {
  TEST_ASSERT_TRUE(
      wifi_dwell_update(&st, "HospitalWiFi", WIFI_DWELL_EPOCH_VALID - 1u));
  TEST_ASSERT_EQUAL_UINT16(0, st.days);
  TEST_ASSERT_EQUAL_UINT32(0, st.firstEpoch);
  TEST_ASSERT_FALSE(
      wifi_dwell_update(&st, "HospitalWiFi", WIFI_DWELL_EPOCH_VALID - 1u));
  TEST_ASSERT_EQUAL_UINT16(0, st.days);
}

// El reloj llega despues de la asociacion: ese instante es el que ancla el
// primer dia, no el arranque a ciegas.
void test_clock_arriving_later_anchors_the_first_day(void) {
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", 0));
  TEST_ASSERT_EQUAL_UINT16(0, st.days);
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0));
  TEST_ASSERT_EQUAL_UINT16(1, st.days);
  TEST_ASSERT_EQUAL_UINT32(T0, st.firstEpoch);
}

// Lo critico de esta feature: si el equipo cambia de red antes de tener
// reloj, los dias NO pueden seguir acreditandose a la red anterior. El reset
// tiene que aplicarse igual, y tiene que persistirse (devolver true).
void test_ssid_change_resets_even_without_clock(void) {
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "TallerMOW", T0));
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "TallerMOW", T0 + DAY));
  TEST_ASSERT_EQUAL_UINT16(2, st.days);

  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", 0));
  TEST_ASSERT_EQUAL_STRING("HospitalWiFi", st.ssid);
  TEST_ASSERT_EQUAL_UINT16(0, st.days);
  TEST_ASSERT_EQUAL_UINT32(0, st.firstEpoch);
}

// --- Entradas de mala fe --------------------------------------------------

void test_null_and_empty_ssid_change_nothing(void) {
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0));
  TEST_ASSERT_FALSE(wifi_dwell_update(&st, NULL, T0 + DAY));
  TEST_ASSERT_FALSE(wifi_dwell_update(&st, "", T0 + DAY));
  TEST_ASSERT_FALSE(wifi_dwell_update(NULL, "Otra", T0 + DAY));
  TEST_ASSERT_EQUAL_STRING("HospitalWiFi", st.ssid);
  TEST_ASSERT_EQUAL_UINT16(1, st.days);
}

// Un SSID son 32 BYTES como maximo, y hay redes que los usan todos.
void test_full_length_ssid_is_stored_whole(void) {
  const char *ssid32 = "01234567890123456789012345678901"; // 32 caracteres
  TEST_ASSERT_EQUAL_UINT32(32, (uint32_t)strlen(ssid32));
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, ssid32, T0));
  TEST_ASSERT_EQUAL_STRING(ssid32, st.ssid);
  // Y en la siguiente pasada tiene que reconocerse como la MISMA red, no
  // como un cambio: si la copia truncara, esto contaria dias de mas.
  TEST_ASSERT_FALSE(wifi_dwell_update(&st, ssid32, T0 + 3600u));
  TEST_ASSERT_EQUAL_UINT16(1, st.days);
}

// Dos redes que solo difieren en el ultimo byte del maximo son redes
// distintas: la comparacion no puede pararse antes de los 32.
void test_ssids_differing_in_the_last_byte_are_different_networks(void) {
  TEST_ASSERT_TRUE(
      wifi_dwell_update(&st, "0123456789012345678901234567890A", T0));
  TEST_ASSERT_TRUE(
      wifi_dwell_update(&st, "0123456789012345678901234567890B", T0));
  TEST_ASSERT_EQUAL_STRING("0123456789012345678901234567890B", st.ssid);
  TEST_ASSERT_EQUAL_UINT16(1, st.days);
}

// --- Span publicado -------------------------------------------------------

void test_span_days_from_first_association(void) {
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0));
  TEST_ASSERT_EQUAL_UINT16(0, wifi_dwell_span_days(&st, T0));
  TEST_ASSERT_EQUAL_UINT16(14, wifi_dwell_span_days(&st, T0 + 14u * DAY));
}

// Sin ancla no hay span, y un reloj que retrocede no puede producir uno
// negativo envuelto a 65535.
void test_span_days_is_zero_without_anchor_or_backwards(void) {
  TEST_ASSERT_EQUAL_UINT16(0, wifi_dwell_span_days(&st, T0));
  TEST_ASSERT_TRUE(wifi_dwell_update(&st, "HospitalWiFi", T0 + 10u * DAY));
  TEST_ASSERT_EQUAL_UINT16(0, wifi_dwell_span_days(&st, T0));
  TEST_ASSERT_EQUAL_UINT16(0, wifi_dwell_span_days(&st, 0));
  TEST_ASSERT_EQUAL_UINT16(0, wifi_dwell_span_days(NULL, T0));
}

// --- Saneado del SSID publicado ------------------------------------------

void test_sanitize_passes_printable_ascii(void) {
  char out[WIFI_DWELL_SSID_MAX + 1];
  wifi_dwell_sanitize_ssid("Hospital WiFi 2.4G", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Hospital WiFi 2.4G", out);
}

// Un SSID son bytes arbitrarios, no una cadena UTF-8. Un byte invalido
// dentro de un payload MQTT es un problema del broker, y se corta aqui.
//
// 0x28 va en medio a proposito: es '(', imprimible, y tiene que sobrevivir
// intacto entre bytes que no lo son. 0x7F (DEL) no es imprimible.
void test_sanitize_replaces_control_and_high_bytes(void) {
  char out[WIFI_DWELL_SSID_MAX + 1];
  const char in[] = {'W', 'i', 'F', 'i', 0x01, (char)0xC3, (char)0x28, 0x7F,
                     '\0'};
  wifi_dwell_sanitize_ssid(in, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("WiFi??(?", out);
}

// La comilla y la barra invertida son datos legitimos de un SSID: las escapa
// el serializador JSON, no este saneado.
void test_sanitize_keeps_quote_and_backslash(void) {
  char out[WIFI_DWELL_SSID_MAX + 1];
  wifi_dwell_sanitize_ssid("a\"b\\c", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("a\"b\\c", out);
}

void test_sanitize_truncates_and_terminates(void) {
  char out[5];
  wifi_dwell_sanitize_ssid("0123456789", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("0123", out);
}

void test_sanitize_handles_empty_and_null(void) {
  char out[8];
  memset(out, 'x', sizeof(out));
  wifi_dwell_sanitize_ssid("", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("", out);
  memset(out, 'x', sizeof(out));
  wifi_dwell_sanitize_ssid(NULL, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("", out);
  // Sin sitio ni para el NUL no se escribe nada: no hay nada que afirmar mas
  // alla de que no reviente.
  wifi_dwell_sanitize_ssid("algo", out, 0);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_empty);
  RUN_TEST(test_first_association_counts_one_day);
  RUN_TEST(test_same_day_reassociation_reports_no_change);
  RUN_TEST(test_next_day_increments_once);
  RUN_TEST(test_day_boundary_is_utc_midnight);
  RUN_TEST(test_non_contiguous_days_count);
  RUN_TEST(test_ssid_change_resets_the_count);
  RUN_TEST(test_unset_clock_counts_no_day);
  RUN_TEST(test_epoch_below_threshold_counts_no_day);
  RUN_TEST(test_clock_arriving_later_anchors_the_first_day);
  RUN_TEST(test_ssid_change_resets_even_without_clock);
  RUN_TEST(test_null_and_empty_ssid_change_nothing);
  RUN_TEST(test_full_length_ssid_is_stored_whole);
  RUN_TEST(test_ssids_differing_in_the_last_byte_are_different_networks);
  RUN_TEST(test_span_days_from_first_association);
  RUN_TEST(test_span_days_is_zero_without_anchor_or_backwards);
  RUN_TEST(test_sanitize_passes_printable_ascii);
  RUN_TEST(test_sanitize_replaces_control_and_high_bytes);
  RUN_TEST(test_sanitize_keeps_quote_and_backslash);
  RUN_TEST(test_sanitize_truncates_and_terminates);
  RUN_TEST(test_sanitize_handles_empty_and_null);
  return UNITY_END();
}

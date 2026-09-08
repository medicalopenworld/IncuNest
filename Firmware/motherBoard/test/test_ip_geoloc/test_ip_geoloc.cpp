#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "modules/util/ip_geoloc.h"
#include "modules/util/tz_source.h"

// La respuesta real de ip-api.com con el fields= que pide el firmware, tal
// como la devuelve para una IP de Addis Abeba (UTC+3 -> offset 10800 s).
static const char *const kGood =
    "{\"status\":\"success\",\"lat\":9.0301,\"lon\":38.7578,\"offset\":10800}";

static float lat, lon;

void setUp(void) {
  lat = -1234.0f;
  lon = -1234.0f;
  tz_source_reset();
}
void tearDown(void) {}

// --- Respuestas buenas ----------------------------------------------------

void test_parses_a_successful_body(void) {
  TEST_ASSERT_TRUE(ip_geoloc_parse(kGood, &lat, &lon));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 9.0301f, lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 38.7578f, lon);
}

void test_parses_negative_coordinates(void) {
  TEST_ASSERT_TRUE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":-33.8688,\"lon\":-70.6693}", &lat,
      &lon));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -33.8688f, lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -70.6693f, lon);
}

void test_parses_integer_coordinates(void) {
  TEST_ASSERT_TRUE(
      ip_geoloc_parse("{\"status\":\"success\",\"lat\":9,\"lon\":39}", &lat,
                      &lon));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 9.0f, lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 39.0f, lon);
}

// El orden de los campos es del servicio, no nuestro.
void test_field_order_does_not_matter(void) {
  TEST_ASSERT_TRUE(ip_geoloc_parse(
      "{\"lon\":38.7578,\"offset\":10800,\"lat\":9.0301,"
      "\"status\":\"success\"}",
      &lat, &lon));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 9.0301f, lat);
}

void test_accepts_the_range_extremes(void) {
  TEST_ASSERT_TRUE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":-90,\"lon\":180}", &lat, &lon));
  TEST_ASSERT_TRUE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":90,\"lon\":-180}", &lat, &lon));
}

// --- Respuestas que hay que rechazar --------------------------------------

// Una consulta fallida puede traer campos igualmente; se exige el success
// explicito en vez de confiar en que no vengan.
void test_rejects_a_failed_lookup(void) {
  TEST_ASSERT_FALSE(ip_geoloc_parse(
      "{\"status\":\"fail\",\"lat\":9.0301,\"lon\":38.7578}", &lat, &lon));
}

void test_rejects_a_body_without_status(void) {
  TEST_ASSERT_FALSE(
      ip_geoloc_parse("{\"lat\":9.0301,\"lon\":38.7578}", &lat, &lon));
}

void test_rejects_out_of_range_values(void) {
  TEST_ASSERT_FALSE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":91.0,\"lon\":38.7}", &lat, &lon));
  TEST_ASSERT_FALSE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":-90.1,\"lon\":38.7}", &lat, &lon));
  TEST_ASSERT_FALSE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":9.0,\"lon\":-200.0}", &lat, &lon));
  TEST_ASSERT_FALSE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":9.0,\"lon\":180.5}", &lat, &lon));
}

void test_rejects_a_missing_coordinate(void) {
  TEST_ASSERT_FALSE(
      ip_geoloc_parse("{\"status\":\"success\",\"lat\":9.0301}", &lat, &lon));
  TEST_ASSERT_FALSE(
      ip_geoloc_parse("{\"status\":\"success\",\"lon\":38.7578}", &lat, &lon));
}

// 0,0 es un punto del golfo de Guinea, pero sobre todo es el "no lo se" de
// media industria. Se descarta igual que el camino GSM descarta 0/0.
void test_rejects_the_null_island(void) {
  TEST_ASSERT_FALSE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":0,\"lon\":0}", &lat, &lon));
}

// Los valores son numeros en el contrato del servicio. Si llegan
// entrecomillados no es la respuesta que esperamos y no se adivina.
void test_rejects_quoted_values(void) {
  TEST_ASSERT_FALSE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":\"9.03\",\"lon\":\"38.75\"}", &lat,
      &lon));
}

void test_rejects_garbage_and_null(void) {
  TEST_ASSERT_FALSE(ip_geoloc_parse("", &lat, &lon));
  TEST_ASSERT_FALSE(ip_geoloc_parse("<html>503</html>", &lat, &lon));
  TEST_ASSERT_FALSE(ip_geoloc_parse(NULL, &lat, &lon));
  TEST_ASSERT_FALSE(ip_geoloc_parse(kGood, NULL, &lon));
  TEST_ASSERT_FALSE(ip_geoloc_parse(kGood, &lat, NULL));
}

// No casar con un campo que solo CONTENGA la palabra. La clave se busca
// entrecomillada, igual que hace tz_parse_ipapi_offset.
void test_does_not_match_similar_field_names(void) {
  TEST_ASSERT_FALSE(ip_geoloc_parse(
      "{\"status\":\"success\",\"latency\":9.03,\"longitude\":38.75}", &lat,
      &lon));
}

// El truncamiento es el caso realista: el buffer es fijo y el deadline de 5 s
// puede cortar la respuesta en cualquier byte.
//
// La invariante no es "rechazar siempre" — un corte justo detras de la coma
// que cierra "lon" deja un par de coordenadas COMPLETO y valido, y aceptarlo
// es correcto. Lo que no puede pasar en NINGUN corte es colar un valor a
// medias, leer mas alla del NUL o dejar las salidas del llamante a medio
// escribir. Eso es lo que se afirma aqui, y es mas fuerte.
void test_no_truncation_produces_a_half_value(void) {
  char buf[128];
  const size_t full = strlen(kGood);
  for (size_t cut = 0; cut <= full; cut++) {
    memcpy(buf, kGood, cut);
    buf[cut] = '\0';
    lat = -1234.0f;
    lon = -1234.0f;
    char msg[48];
    snprintf(msg, sizeof(msg), "corte en %u", (unsigned)cut);
    if (ip_geoloc_parse(buf, &lat, &lon)) {
      // Si acepta, tiene que ser el valor entero y exacto.
      TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.0001f, 9.0301f, lat, msg);
      TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.0001f, 38.7578f, lon, msg);
    } else {
      TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.0001f, -1234.0f, lat, msg);
      TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.0001f, -1234.0f, lon, msg);
    }
  }
}

// Y el corte a mitad de cifra, que es el que de verdad podria colar un valor
// desplazado, se rechaza: el numero tiene que estar terminado por la
// sintaxis del objeto.
void test_rejects_a_cut_mid_number(void) {
  TEST_ASSERT_FALSE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":9.0301,\"lon\":38.757", &lat, &lon));
  TEST_ASSERT_FALSE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":9.0301,\"lon\":38.7578", &lat, &lon));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -1234.0f, lon);
}

void test_leaves_outputs_untouched_on_rejection(void) {
  TEST_ASSERT_FALSE(ip_geoloc_parse(
      "{\"status\":\"success\",\"lat\":91.0,\"lon\":38.7}", &lat, &lon));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -1234.0f, lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -1234.0f, lon);
}

// --- Regresion: la zona horaria sigue saliendo del mismo cuerpo -----------

// Ampliar el fields= no puede romper lo que esa consulta ya hacia. Con las
// coordenadas delante, el offset tiene que seguir parseandose igual.
void test_timezone_still_parses_from_the_extended_body(void) {
  int quarters = 0;
  TEST_ASSERT_TRUE(tz_parse_ipapi_offset(kGood, &quarters));
  TEST_ASSERT_EQUAL_INT(12, quarters); // 10800 s = UTC+3 = 12 cuartos
}

// Y al reves: un cuerpo con offset pero sin coordenadas resuelve la zona
// aunque no haya posicion.
void test_timezone_parses_when_the_position_is_absent(void) {
  int quarters = 0;
  TEST_ASSERT_TRUE(
      tz_parse_ipapi_offset("{\"status\":\"success\",\"offset\":3600}",
                            &quarters));
  TEST_ASSERT_EQUAL_INT(4, quarters);
  TEST_ASSERT_FALSE(ip_geoloc_parse("{\"status\":\"success\",\"offset\":3600}",
                                    &lat, &lon));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_parses_a_successful_body);
  RUN_TEST(test_parses_negative_coordinates);
  RUN_TEST(test_parses_integer_coordinates);
  RUN_TEST(test_field_order_does_not_matter);
  RUN_TEST(test_accepts_the_range_extremes);
  RUN_TEST(test_rejects_a_failed_lookup);
  RUN_TEST(test_rejects_a_body_without_status);
  RUN_TEST(test_rejects_out_of_range_values);
  RUN_TEST(test_rejects_a_missing_coordinate);
  RUN_TEST(test_rejects_the_null_island);
  RUN_TEST(test_rejects_quoted_values);
  RUN_TEST(test_rejects_garbage_and_null);
  RUN_TEST(test_does_not_match_similar_field_names);
  RUN_TEST(test_no_truncation_produces_a_half_value);
  RUN_TEST(test_rejects_a_cut_mid_number);
  RUN_TEST(test_leaves_outputs_untouched_on_rejection);
  RUN_TEST(test_timezone_still_parses_from_the_extended_body);
  RUN_TEST(test_timezone_parses_when_the_position_is_absent);
  return UNITY_END();
}

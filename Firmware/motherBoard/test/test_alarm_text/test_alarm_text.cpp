#include <unity.h>
#include <stdio.h>
#include <string.h>

#include "alarm_ids.h"
#include "alarm_policy.h"
#include "alarm_text.h"

// Todos los idiomas que soporta el equipo. Cada uno que entra aqui somete sus
// cadenas a los limites de ancho del protocolo, que es donde duele: el
// portugues es mas largo que el ingles palabra por palabra ("AQUECEDOR" por
// "HEATER") y el titulo compite con la marca de prioridad por 29 caracteres.
static const Language kLanguages[] = {SPANISH, ENGLISH, FRENCH, PORTUGUESE};
static const unsigned kNumLanguages =
    sizeof(kLanguages) / sizeof(kLanguages[0]);

void setUp(void) {}
void tearDown(void) {}

// El fallo que motiva este test: las descripciones que escribio la rama del
// sistema de alarmas son frases clinicas de hasta 43 caracteres, y el display
// las parseaba con un ancho de 31 hardcodeado. sscanf paraba a los 31, no
// encontraba la coma que exige el formato, devolvia 3 campos en vez de 4 y la
// linea entera se DESCARTABA: 12 de las 16 alarmas no llegaban a aparecer en
// pantalla en espanol — ni tarjeta, ni identidad, ni prioridad — mientras el
// zumbador sonaba. No hay segunda via: la sincronizacion por bitmask del
// display solo apaga slots, nunca los enciende.
void test_every_title_fits_the_protocol_field(void) {
  for (int id = ALARM_NONE; id <= ALARM_COUNT; ++id) {
    for (unsigned l = 0; l < kNumLanguages; ++l) {
      const char *t = alarm_title_text((AlarmId)id, kLanguages[l]);
      TEST_ASSERT_NOT_NULL(t);
      TEST_ASSERT_TRUE_MESSAGE(strlen(t) <= (size_t)ALARM_TITLE_MAX_CHARS, t);
    }
  }
}

void test_every_action_line_fits_the_protocol_field(void) {
  for (int id = ALARM_NONE; id <= ALARM_COUNT; ++id) {
    for (unsigned l = 0; l < kNumLanguages; ++l) {
      const char *d = alarm_action_text((AlarmId)id, kLanguages[l]);
      TEST_ASSERT_NOT_NULL(d);
      TEST_ASSERT_TRUE_MESSAGE(strlen(d) <= (size_t)ALARM_DESC_MAX_CHARS, d);
    }
  }
}

// Segundo limite, esta vez en el emisor: sendAlarmUSB() compone la linea
// entera en un buffer de ALARM_LINE_BUF_SIZE y snprintf trunca en silencio.
// Una linea truncada llega al display sin su ultimo campo y se descarta igual.
void test_every_composed_line_fits_the_sender_buffer(void) {
  char line[512];
  for (int id = ALARM_NONE; id <= ALARM_COUNT; ++id) {
    for (unsigned l = 0; l < kNumLanguages; ++l) {
      const int n = snprintf(line, sizeof(line), "CTRL,ALM,%d,%s,%s,%d\n", id,
                             alarm_title_text((AlarmId)id, kLanguages[l]),
                             alarm_action_text((AlarmId)id, kLanguages[l]), 1);
      TEST_ASSERT_TRUE(n > 0);
      TEST_ASSERT_TRUE_MESSAGE(n < ALARM_LINE_BUF_SIZE, line);
    }
  }
}

// La coma es el separador de campos del protocolo: una sola coma dentro de un
// texto parte el campo en dos y rompe el parseo igual que pasarse de ancho.
// El salto de linea termina la trama, asi que tampoco puede aparecer dentro.
void test_no_alarm_text_contains_a_field_separator(void) {
  for (int id = ALARM_NONE; id <= ALARM_COUNT; ++id) {
    for (unsigned l = 0; l < kNumLanguages; ++l) {
      const char *texts[2] = {alarm_title_text((AlarmId)id, kLanguages[l]),
                              alarm_action_text((AlarmId)id, kLanguages[l])};
      for (unsigned t = 0; t < 2; ++t) {
        TEST_ASSERT_NULL_MESSAGE(strchr(texts[t], ','), texts[t]);
        TEST_ASSERT_NULL_MESSAGE(strchr(texts[t], '\n'), texts[t]);
        TEST_ASSERT_NULL_MESSAGE(strchr(texts[t], '\r'), texts[t]);
      }
    }
  }
}

// Las fuentes del display no tienen glifos fuera del ASCII imprimible: un
// acento saldria como basura justo en el texto que hay que leer con prisa.
void test_every_alarm_text_is_printable_ascii(void) {
  for (int id = ALARM_NONE; id <= ALARM_COUNT; ++id) {
    for (unsigned l = 0; l < kNumLanguages; ++l) {
      const char *texts[2] = {alarm_title_text((AlarmId)id, kLanguages[l]),
                              alarm_action_text((AlarmId)id, kLanguages[l])};
      for (unsigned t = 0; t < 2; ++t) {
        for (const char *c = texts[t]; *c; ++c) {
          TEST_ASSERT_TRUE_MESSAGE((unsigned char)*c >= 0x20 &&
                                       (unsigned char)*c < 0x7F,
                                   texts[t]);
        }
      }
    }
  }
}

// Ninguna condicion puede quedarse sin texto: una alarma sin identidad en
// pantalla es una alarma que el operador no puede atender.
void test_no_alarm_text_is_empty(void) {
  for (int id = ALARM_NONE + 1; id < ALARM_COUNT; ++id) {
    for (unsigned l = 0; l < kNumLanguages; ++l) {
      TEST_ASSERT_TRUE(strlen(alarm_title_text((AlarmId)id, kLanguages[l])) > 0);
      TEST_ASSERT_TRUE(strlen(alarm_action_text((AlarmId)id, kLanguages[l])) >
                       0);
    }
  }
}

// Un id fuera de rango tampoco puede devolver NULL: sendAlarmUSB() lo pasaria
// a snprintf("%s") directamente.
void test_out_of_range_ids_still_return_a_text(void) {
  TEST_ASSERT_NOT_NULL(alarm_title_text((AlarmId)999, SPANISH));
  TEST_ASSERT_NOT_NULL(alarm_action_text((AlarmId)999, SPANISH));
  TEST_ASSERT_NOT_NULL(alarm_title_text(ALARM_NONE, FRENCH));
  TEST_ASSERT_NOT_NULL(alarm_action_text(ALARM_NONE, FRENCH));
}

// IEC 60601-1-8 6.3.2.2.2 exige que la senal visual de 1 m identifique la
// condicion **y su prioridad**. El protocolo no lleva campo de prioridad, asi
// que la marca antepuesta al titulo es hoy el unico vehiculo, y la clausula
// ofrece literalmente esta convencion de uno, dos o tres signos.
void test_priority_mark_matches_the_assigned_priority(void) {
  for (int id = ALARM_NONE + 1; id < ALARM_COUNT; ++id) {
    const char *mark = alarm_priority_mark((AlarmId)id);
    TEST_ASSERT_NOT_NULL(mark);
    switch (alarm_priority((AlarmId)id)) {
      case ALARM_PRIORITY_HIGH:
        TEST_ASSERT_EQUAL_STRING("!!!", mark);
        break;
      case ALARM_PRIORITY_MEDIUM:
        TEST_ASSERT_EQUAL_STRING("!!", mark);
        break;
      default:
        TEST_ASSERT_EQUAL_STRING("!", mark);
        break;
    }
  }
}

// El titulo viaja con la marca delante, y ese compuesto es lo que tiene que
// caber en el campo del protocolo. Medir solo el titulo desnudo dejaria pasar
// justo el fallo que dejo 12 de 16 alarmas invisibles: un campo que se pasa de
// ancho hace que el display descarte la linea entera, no que la recorte.
void test_title_with_priority_mark_still_fits_the_protocol_field(void) {
  for (int id = ALARM_NONE + 1; id < ALARM_COUNT; ++id) {
    for (unsigned l = 0; l < kNumLanguages; ++l) {
      char composed[128];
      snprintf(composed, sizeof(composed), "%s %s",
               alarm_priority_mark((AlarmId)id),
               alarm_title_text((AlarmId)id, kLanguages[l]));
      TEST_ASSERT_TRUE_MESSAGE(
          strlen(composed) <= (size_t)ALARM_TITLE_MAX_CHARS, composed);
    }
  }
}

// Cada idioma soportado tiene que traer texto PROPIO para cada condicion, no
// el ingles de reserva.
//
// El fallback existe para que un idioma a medio traducir no deje la pantalla
// en blanco, pero no puede ser el estado final: un operador que ha puesto el
// equipo en portugues y lee la alarma en ingles esta en el mismo sitio que si
// no hubiera texto. Y el fallback es SILENCIOSO — compila, cabe, es ASCII y no
// esta vacio, asi que ninguno de los otros tests de este fichero lo detecta.
// Este es el unico que distingue "traducido" de "cae en ingles".
void test_every_language_has_its_own_text_for_every_alarm(void) {
  for (int id = ALARM_NONE + 1; id < ALARM_COUNT; ++id) {
    for (unsigned l = 0; l < kNumLanguages; ++l) {
      if (kLanguages[l] == ENGLISH) {
        continue;
      }
      char msg[192];
      const char *title = alarm_title_text((AlarmId)id, kLanguages[l]);
      snprintf(msg, sizeof(msg), "id=%d lang=%d titulo sin traducir: %s", id,
               (int)kLanguages[l], title);
      TEST_ASSERT_TRUE_MESSAGE(
          strcmp(title, alarm_title_text((AlarmId)id, ENGLISH)) != 0, msg);

      const char *action = alarm_action_text((AlarmId)id, kLanguages[l]);
      snprintf(msg, sizeof(msg), "id=%d lang=%d accion sin traducir: %s", id,
               (int)kLanguages[l], action);
      TEST_ASSERT_TRUE_MESSAGE(
          strcmp(action, alarm_action_text((AlarmId)id, ENGLISH)) != 0, msg);
    }
  }
}

// El fallback sigue existiendo, y lo que protege de verdad es un valor de
// idioma que no corresponde a ningun idioma soportado: `in3.language` viene de
// NVS y del protocolo serie, asi que puede llegar cualquier numero. Se
// comprueba con NUM_LANGUAGES, que nunca sera un idioma.
void test_unknown_language_falls_back_to_english(void) {
  const Language bogus = (Language)NUM_LANGUAGES;
  TEST_ASSERT_EQUAL_STRING(alarm_title_text(ALARM_FAN_FAILURE, ENGLISH),
                           alarm_title_text(ALARM_FAN_FAILURE, bogus));
  TEST_ASSERT_EQUAL_STRING(alarm_action_text(ALARM_FAN_FAILURE, ENGLISH),
                           alarm_action_text(ALARM_FAN_FAILURE, bogus));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_every_title_fits_the_protocol_field);
  RUN_TEST(test_every_action_line_fits_the_protocol_field);
  RUN_TEST(test_every_composed_line_fits_the_sender_buffer);
  RUN_TEST(test_no_alarm_text_contains_a_field_separator);
  RUN_TEST(test_every_alarm_text_is_printable_ascii);
  RUN_TEST(test_no_alarm_text_is_empty);
  RUN_TEST(test_out_of_range_ids_still_return_a_text);
  RUN_TEST(test_priority_mark_matches_the_assigned_priority);
  RUN_TEST(test_title_with_priority_mark_still_fits_the_protocol_field);
  RUN_TEST(test_every_language_has_its_own_text_for_every_alarm);
  RUN_TEST(test_unknown_language_falls_back_to_english);
  return UNITY_END();
}

#include <unity.h>
#include <stdio.h>
#include <string.h>

#include "alarm_ids.h"
#include "alarm_text.h"

// Los idiomas con traduccion propia. PORTUGUESE existe en el enum pero cae en
// el texto ingles, asi que no aporta cadenas nuevas que medir.
static const Language kLanguages[] = {SPANISH, ENGLISH, FRENCH};
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

// Un idioma sin traduccion propia cae en ingles, no en cadena vacia.
void test_untranslated_language_falls_back_to_english(void) {
  TEST_ASSERT_EQUAL_STRING(alarm_title_text(ALARM_FAN_FAILURE, ENGLISH),
                           alarm_title_text(ALARM_FAN_FAILURE, PORTUGUESE));
  TEST_ASSERT_EQUAL_STRING(alarm_action_text(ALARM_FAN_FAILURE, ENGLISH),
                           alarm_action_text(ALARM_FAN_FAILURE, PORTUGUESE));
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
  RUN_TEST(test_untranslated_language_falls_back_to_english);
  return UNITY_END();
}

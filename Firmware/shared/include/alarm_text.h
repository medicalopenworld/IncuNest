#pragma once
#include "alarm_ids.h"
#include "control_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Textos de operador de cada condicion de alarma, en los idiomas que soporta
// el equipo. Viven en shared/ y no en security.cpp por una razon concreta:
// tienen que caber en los campos de la linea CTRL,ALM (ALARM_TITLE_MAX_CHARS y
// ALARM_DESC_MAX_CHARS, alarm_ids.h) o el display descarta la linea entera, y
// esa comprobacion solo se puede automatizar si las cadenas son alcanzables
// desde el entorno de test nativo. Ver test/test_alarm_text/.
//
// Sin acentos ni nada fuera de ASCII imprimible: las fuentes del display no
// tienen esos glifos y saldrian como basura justo cuando mas importa leerlos.
// Sin comas: la coma es el separador de campos del protocolo.
//
// Nunca devuelven NULL. Un id desconocido devuelve un texto generico.

// Identidad de la condicion: QUE ha pasado. Es el campo <titulo>.
const char *alarm_title_text(AlarmId id, Language lang);

// Linea de accion: QUE HACER. La lee personal clinico junto a la incubadora,
// asi que prima decir la accion sobre describir el diagnostico. Es el campo
// <descripcion>.
const char *alarm_action_text(AlarmId id, Language lang);

#ifdef __cplusplus
}
#endif

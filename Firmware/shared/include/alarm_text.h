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

// Marca de prioridad: "!" BAJA, "!!" MEDIA, "!!!" ALTA.
//
// IEC 60601-1-8 6.3.2.2.2 exige que la senal visual de 1 m identifique "the
// specific ALARM CONDITION **and its priority**", y ofrece literalmente esta
// convencion de uno, dos o tres elementos como forma valida de indicarla. El
// titulo por si solo identifica la condicion pero no su prioridad, y el
// protocolo CTRL,ALM no lleva campo de prioridad, asi que hoy el texto es el
// unico vehiculo que existe: sin esta marca, la senal de 1 m no cumple.
//
// Va antepuesta al titulo al componer la linea del protocolo, no incrustada en
// los literales, para que las traducciones sigan siendo solo la identidad de
// la condicion y la prioridad salga de alarm_priority(), que es su unica
// fuente de verdad. Si algun dia el protocolo lleva la prioridad como campo
// propio y el display la pinta con color, esta marca puede retirarse de aqui
// sin tocar ni una traduccion.
const char *alarm_priority_mark(AlarmId id);

#ifdef __cplusplus
}
#endif

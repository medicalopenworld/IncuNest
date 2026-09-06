#pragma once

// Catalogo de textos de la interfaz.
//
// Antes cada cadena visible vivia empotrada en su punto de uso, como un array
// de tres literales (`{"HUMEDAD", "HUMIDITY", "HUMIDITE"}`) o como una cascada
// de ternarios sobre `g_lang`, repartidos por `UITask.cpp` y por los seis
// ficheros de `ui/`. Anadir un idioma obligaba a barrer ~200 sitios y cada
// olvido se manifestaba solo al mirar esa pantalla concreta con ese idioma.
//
// Ahora las cadenas viven en `i18n_strings.def`, un fichero unico que se
// expande dos veces con la misma macro (X-macro): aqui para el enum de ids, y
// en `i18n.cpp` para la tabla. Enum y tabla no pueden desalinearse porque
// salen de la misma lista.
//
// Anadir un idioma = una columna nueva en el `.def` + una entrada en
// `ui_lang_t`. Ningun punto de llamada cambia.
//
// Solo lo consume codigo C++ (todo `src/ui/` y `src/tasks/` salvo el
// `ui_helpers.c` generado por SquareLine, que no pinta texto propio), asi que
// no lleva envoltorio `extern "C"`.

#include "control_types.h" // Language: orden canonico del protocolo y de NVS

// -----------------------------------------------------------------
// Idiomas
// -----------------------------------------------------------------
//
// El valor numerico viaja por el protocolo serie (campo `language` de
// `protocol.h`), se guarda en NVS y sube a ThingsBoard como `UI_language`, asi
// que EL ORDEN ES PARTE DEL CONTRATO: reordenar aqui cambia de idioma a los
// equipos ya desplegados. Es el mismo orden que `Language` en
// `shared/include/control_types.h`, y los static_assert de abajo lo fijan.
typedef enum {
  LANG_ES = 0,
  LANG_EN = 1,
  LANG_FR = 2,
  LANG_PT = 3,
  UI_LANG_COUNT
} ui_lang_t;

// Idioma de reserva: el que se usa cuando falta una traduccion, o cuando el
// valor guardado en NVS ya no corresponde a ningun idioma soportado.
#define UI_LANG_FALLBACK LANG_EN

// El orden de `ui_lang_t` debe seguir al de `Language` en shared/: es el mismo
// numero el que viaja por el protocolo y el que se persiste en NVS.
static_assert((int)LANG_ES == (int)SPANISH, "ui_lang_t desalineado con Language");
static_assert((int)LANG_EN == (int)ENGLISH, "ui_lang_t desalineado con Language");
static_assert((int)LANG_FR == (int)FRENCH, "ui_lang_t desalineado con Language");
static_assert((int)LANG_PT == (int)PORTUGUESE, "ui_lang_t desalineado con Language");
static_assert((int)UI_LANG_COUNT <= (int)NUM_LANGUAGES,
              "el HMI declara mas idiomas que shared/control_types.h");

// Idioma activo. Lo define `i18n.cpp`; lo cambia `UI_ApplyLanguage()`.
extern ui_lang_t g_lang;

// -----------------------------------------------------------------
// Ids de cadena
// -----------------------------------------------------------------
typedef enum {
#define UI_STR(id, ...) id,
#include "ui/i18n_strings.def"
#undef UI_STR
  UI_STR_COUNT
} ui_str_id_t;

// -----------------------------------------------------------------
// Tamano de los buffers que alojan texto traducido
// -----------------------------------------------------------------
//
// Un buffer de pila dimensionado a ojo para el texto en un idioma se queda
// corto en cuanto otro es mas largo, y snprintf trunca EN SILENCIO: no hay
// error, no hay aviso, solo una etiqueta cortada que unicamente se ve abriendo
// esa pantalla en ese idioma.
//
// Paso de verdad: el reloj de la cabecera tenia `char nowTime[8]`, medido para
// "HH:MM", y ahi dentro cae tambien el texto de STR_NO_TIME. En espanol
// "Sin hora" (8) salia como "Sin hor" y en frances "Sans heure" (10) como
// "Sans he"; solo el ingles cabia, y de casualidad. Con amarico seria peor: en
// UTF-8 cada silaba etiope ocupa 3 bytes, asi que el corte caeria a mitad de
// secuencia y pintaria basura en vez de una palabra incompleta.
//
// `UI_StrBufBytes(id)` da el tamano correcto sacado del propio catalogo, asi
// que un idioma nuevo lo actualiza solo, sin que nadie tenga que acordarse.
namespace ui_i18n_detail {

constexpr unsigned len(const char *s) {
  unsigned n = 0;
  while (s[n]) {
    ++n;
  }
  return n;
}

// Un parametro por idioma, igual que la macro de la tabla en i18n.cpp: asi
// anadir un idioma sin tocar esto es un error de compilacion, no un tamano
// calculado de menos.
constexpr unsigned longest(const char *es, const char *en, const char *fr,
                           const char *pt) {
  unsigned m = len(es);
  if (len(en) > m) m = len(en);
  if (len(fr) > m) m = len(fr);
  if (len(pt) > m) m = len(pt);
  return m;
}

// Bytes de la traduccion mas larga de cada cadena, sin contar el terminador.
// Se calcula entero en compilacion; los literales no se emiten por esto.
inline constexpr unsigned kMaxLen[] = {
#define UI_STR(id, es, en, fr, pt) longest(es, en, fr, pt),
#include "ui/i18n_strings.def"
#undef UI_STR
};

} // namespace ui_i18n_detail

// Bytes minimos de un buffer capaz de alojar cualquier traduccion de `id`,
// terminador incluido. Sirve para dimensionar el array directamente:
//
//   char buf[UI_StrBufBytes(STR_NO_TIME)];
constexpr unsigned UI_StrBufBytes(ui_str_id_t id) {
  return ui_i18n_detail::kMaxLen[(unsigned)id] + 1u;
}

// Texto en el idioma activo. Nunca devuelve NULL: si falta la traduccion cae
// al ingles, y si tambien falta, a cadena vacia.
//
// El puntero apunta a memoria estatica y es valido para siempre, asi que se
// puede pasar a `lv_dropdown_set_options()` o guardarlo en el mapa de un
// `lv_btnmatrix`, que conservan el puntero en vez de copiar.
const char *UI_Str(ui_str_id_t id);

// Igual, pero en un idioma concreto. Para el codigo que ya recibe el idioma
// por parametro en vez de leer el global.
const char *UI_StrIn(ui_str_id_t id, ui_lang_t lang);

// true si `v` es un indice de idioma soportado por esta version del firmware.
bool UI_LangIsValid(int v);

// Azucar para el caso normal: `TR(STR_HUMIDITY)`.
#define TR(id) UI_Str(id)

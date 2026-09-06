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

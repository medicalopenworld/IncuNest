#include "ui/i18n.h"

// Tabla de traducciones.
//
// Se genera del mismo `i18n_strings.def` que produjo el enum `ui_str_id_t`, asi
// que fila y id no pueden desincronizarse: si alguien inserta una cadena en
// medio del `.def`, enum y tabla se desplazan a la vez.
static const char *const kCatalog[UI_STR_COUNT][UI_LANG_COUNT] = {
#define UI_STR(id, ...) {__VA_ARGS__},
#include "ui/i18n_strings.def"
#undef UI_STR
};

// Idioma activo de la interfaz.
//
// Vive aqui, junto a la tabla que consulta, en vez de en `UITask.cpp`: lo leen
// tanto la tarea de UI como los seis overlays de `ui/`, y antes cada uno se lo
// declaraba `extern` por su cuenta.
//
// Arranca en ingles a proposito: es el valor de reserva y es lo que se ve si
// NVS todavia no tiene nada guardado (primer arranque) o si el valor guardado
// ya no corresponde a un idioma soportado.
ui_lang_t g_lang = UI_LANG_FALLBACK;

bool UI_LangIsValid(int v) { return v >= 0 && v < (int)UI_LANG_COUNT; }

const char *UI_StrIn(ui_str_id_t id, ui_lang_t lang) {
  if ((unsigned)id >= (unsigned)UI_STR_COUNT) {
    return "";
  }
  if (!UI_LangIsValid((int)lang)) {
    lang = UI_LANG_FALLBACK;
  }

  // Una celda vacia significa "sin traducir todavia": se muestra el ingles en
  // vez de un hueco. Eso permite anadir un idioma al `.def` rellenando su
  // columna por partes, sin dejar la pantalla a medias por el camino.
  const char *s = kCatalog[id][lang];
  if (s && *s) {
    return s;
  }
  s = kCatalog[id][UI_LANG_FALLBACK];
  return s ? s : "";
}

const char *UI_Str(ui_str_id_t id) { return UI_StrIn(id, g_lang); }

#include "ui/i18n.h"

// Tabla de traducciones.
//
// Se genera del mismo `i18n_strings.def` que produjo el enum `ui_str_id_t`, asi
// que fila e id no pueden desincronizarse: si alguien inserta una cadena en
// medio del `.def`, enum y tabla se desplazan a la vez.
//
// La macro nombra las columnas una a una en vez de usar `...`: asi una fila a
// la que le falte un idioma es un ERROR DE COMPILACION. Con `{__VA_ARGS__}` en
// un array de UI_LANG_COUNT elementos, los que faltasen se rellenarian con
// NULL sin una sola queja, y el fallo solo se veria al poner el equipo en ese
// idioma y llegar a esa pantalla. Anadir un idioma cuesta un parametro mas
// aqui, al lado de la entrada nueva de `ui_lang_t`.
static constexpr const char *const kCatalog[UI_STR_COUNT][UI_LANG_COUNT] = {
#define UI_STR(id, es, en, fr, pt) {es, en, fr, pt},
#include "ui/i18n_strings.def"
#undef UI_STR
};

// Las fuentes Montserrat integradas de LVGL solo traen ASCII 32-126: una letra
// acentuada no da error, se pinta como caja vacia, y eso solo se descubre
// mirando la pantalla en el idioma afectado. Aqui se convierte en un error de
// compilacion. Si algun dia se carga una fuente con mas cobertura, este
// static_assert es el sitio donde consta por que se escribia todo sin tildes.
static constexpr bool catalog_is_ascii() {
  for (int id = 0; id < (int)UI_STR_COUNT; ++id) {
    for (int lang = 0; lang < (int)UI_LANG_COUNT; ++lang) {
      const char *s = kCatalog[id][lang];
      if (!s) {
        continue;
      }
      for (; *s; ++s) {
        const unsigned char c = (unsigned char)*s;
        // El salto de linea si vale: varias etiquetas son de dos o tres
        // lineas y LVGL las parte por el. Cualquier otro control, no.
        if (c == '\n') {
          continue;
        }
        if (c < 0x20 || c > 0x7E) {
          return false;
        }
      }
    }
  }
  return true;
}
static_assert(catalog_is_ascii(),
              "i18n_strings.def tiene un caracter fuera de ASCII 32-126: las "
              "fuentes Montserrat cargadas no pueden pintarlo");

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

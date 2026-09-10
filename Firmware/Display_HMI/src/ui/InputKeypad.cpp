#include "ui/InputKeypad.h"

#include <cstring>

namespace {

// Solo letras, en mayusculas: sin digitos y, sobre todo, sin ninguna tecla de
// coma. El protocolo es CSV, asi que el caracter es inalcanzable por
// construccion desde aqui (InputKeypad_StripCommasCb queda como defensa en
// profundidad).
const char *KB_LETTERS_MAP[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    " ", ""};
// 28 botones: 10 + 9 + 8 + 1. Tiene que coincidir con KB_LETTERS_MAP.
const lv_btnmatrix_ctrl_t KB_LETTERS_CTRL[28] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1};

// Solo digitos: sin punto decimal, sin signo, sin tecla de cambio de modo.
const char *KB_DIGITS_MAP[] = {"1", "2", "3", "\n",
                               "4", "5", "6", "\n",
                               "7", "8", "9", "\n",
                               LV_SYMBOL_BACKSPACE, "0", ""};
// 11 botones: 3 + 3 + 3 + 2.
const lv_btnmatrix_ctrl_t KB_DIGITS_CTRL[11] = {1, 1, 1, 1, 1, 1,
                                                1, 1, 1, 1, 1};

void onKeyPress(lv_event_t *e) {
  lv_obj_t *bm = lv_event_get_target(e);
  lv_obj_t *ta = (lv_obj_t *)lv_event_get_user_data(e);
  if (!bm || !ta) return;
  const char *txt =
      lv_btnmatrix_get_btn_text(bm, lv_btnmatrix_get_selected_btn(bm));
  if (!txt) return;
  if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
    lv_textarea_del_char(ta);
  } else {
    lv_textarea_add_text(ta, txt);
  }
}

}  // namespace

lv_obj_t *InputKeypad_Create(lv_obj_t *parent, lv_obj_t *ta, bool digits) {
  // Deliberadamente un lv_btnmatrix y NO un lv_keyboard: LVGL 8.3 guarda los
  // mapas del teclado en un global estatico del fichero (`kb_map[mode]`), asi
  // que lv_keyboard_set_map() reescribiria el mapa de TODOS los teclados de
  // la aplicacion — incluido ui_Keyboard1, el teclado persistente de
  // credenciales WiFi, que perderia sus digitos. lv_btnmatrix guarda el mapa
  // por instancia.
  lv_obj_t *kb = lv_btnmatrix_create(parent);
  lv_btnmatrix_set_map(kb, digits ? KB_DIGITS_MAP : KB_LETTERS_MAP);
  lv_btnmatrix_set_ctrl_map(kb, digits ? KB_DIGITS_CTRL : KB_LETTERS_CTRL);
  lv_obj_set_size(kb, digits ? 420 : 750, 250);
  lv_obj_set_style_text_font(kb, &lv_font_montserrat_20, LV_PART_ITEMS);
  lv_obj_add_event_cb(kb, onKeyPress, LV_EVENT_VALUE_CHANGED, ta);
  return kb;
}

void InputKeypad_StripCommasCb(lv_event_t *e) {
  lv_obj_t *ta = lv_event_get_target(e);
  const char *txt = lv_textarea_get_text(ta);
  if (!txt || !strchr(txt, ',')) return;
  // Mas largo que cualquier nombre admitido (23): si un textarea sin tope
  // llegara aqui, se trunca en vez de desbordar.
  char clean[64];
  size_t j = 0;
  for (size_t i = 0; txt[i] && j < sizeof(clean) - 1; i++) {
    if (txt[i] != ',') clean[j++] = txt[i];
  }
  clean[j] = '\0';
  lv_textarea_set_text(ta, clean);
}

bool InputKeypad_ReadNumber(lv_obj_t *ta, uint32_t lo, uint32_t hi,
                            uint32_t *out) {
  if (!ta || !out) return false;
  const char *txt = lv_textarea_get_text(ta);
  if (!txt || txt[0] == '\0') return false;
  uint32_t v = 0;
  for (size_t i = 0; txt[i]; i++) {
    if (txt[i] < '0' || txt[i] > '9') return false;
    v = v * 10u + (uint32_t)(txt[i] - '0');
    if (v > 99999u) return false;
  }
  if (v < lo || v > hi) return false;
  *out = v;
  return true;
}

bool InputKeypad_ReadName(lv_obj_t *ta, char *out, size_t len) {
  if (!ta || !out || len == 0) return false;
  const char *txt = lv_textarea_get_text(ta);
  if (!txt) return false;
  while (*txt == ' ') txt++;
  size_t n = strlen(txt);
  while (n > 0 && txt[n - 1] == ' ') n--;
  if (n == 0) {
    out[0] = '\0';
    return false;
  }
  if (n > len - 1) n = len - 1;
  memcpy(out, txt, n);
  out[n] = '\0';
  return true;
}

#include "ui/TimeDialog.h"

#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "UITask.h"
#include "main.h"
#include "ui.h"

// --- Shared state owned by UITask.cpp (same pattern BabyWizard.cpp uses) ---
namespace {

// La mascara que ve el operador. Los huecos se rellenan de izquierda a
// derecha a medida que teclea, asi que el propio campo indica por donde va
// sin necesidad de cursor.
const char MASK_TPL[] = "XX/XX/XX XX:XX";
constexpr uint8_t DIGIT_COUNT = 10;
// Indice dentro de MASK_TPL de cada hueco, en orden de tecleo:
// DD, MM, AA, HH, MM.
const uint8_t MASK_POS[DIGIT_COUNT] = {0, 1, 3, 4, 6, 7, 9, 10, 12, 13};

// Ano de 2 cifras -> 20AA, con el mismo rango que aceptaba el spinbox de 4
// cifras al que sustituye esta mascara.
constexpr int YEAR_BASE = 2000;
constexpr int YEAR_MIN = 2021;
constexpr int YEAR_MAX = 2099;

char s_digits[DIGIT_COUNT];
uint8_t s_count = 0;
bool s_open = false;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_card = nullptr;
lv_obj_t *s_content = nullptr;
// Hijo de la tarjeta y no del contenido, para que sobreviva al lv_obj_clean()
// de cada apertura: es la unica salida del dialogo sin tocar nada.
lv_obj_t *s_closeBtn = nullptr;
lv_obj_t *s_maskLbl = nullptr;
lv_obj_t *s_resultLbl = nullptr;

// Geometria copiada del paso de peso del BabyWizard (CARD_W_BIG/CARD_H_BIG y
// su teclado de 420x250): el operador ya conoce ese cuadro, y este pide lo
// mismo —una cifra tras otra en un teclado numerico—, asi que no hay motivo
// para que se vea distinto.
constexpr lv_coord_t CARD_W = 780, CARD_H = 460;

// lv_btnmatrix y no lv_keyboard, por el mismo motivo que en BabyWizard.cpp:
// en LVGL 8.3 el keymap de lv_keyboard vive en un global de fichero
// (kb_map[mode]), asi que lv_keyboard_set_map() se lo cambiaria tambien al
// teclado permanente de credenciales WiFi (ui_Keyboard1).
const char *KB_DIGITS_MAP[] = {"1", "2", "3", "\n",
                               "4", "5", "6", "\n",
                               "7", "8", "9", "\n",
                               LV_SYMBOL_BACKSPACE, "0", ""};
// 11 botones: 3 + 3 + 3 + 2. Debe cuadrar con KB_DIGITS_MAP.
const lv_btnmatrix_ctrl_t KB_DIGITS_CTRL[11] = {1, 1, 1, 1, 1, 1,
                                                1, 1, 1, 1, 1};

lv_obj_t *makeBtn(lv_obj_t *parent, const char *text, lv_event_cb_t cb,
                  lv_color_t bg) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  return btn;
}

void renderMask() {
  if (!s_maskLbl) return;
  char buf[sizeof(MASK_TPL)];
  memcpy(buf, MASK_TPL, sizeof(MASK_TPL));
  for (uint8_t i = 0; i < s_count; i++) buf[MASK_POS[i]] = s_digits[i];
  lv_label_set_text(s_maskLbl, buf);
}

void clearResult() {
  if (s_resultLbl) lv_label_set_text(s_resultLbl, "");
}

void closeDialog() {
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  if (s_content) lv_obj_clean(s_content);
  s_maskLbl = nullptr;
  s_resultLbl = nullptr;
  s_open = false;
}

void onClose(lv_event_t *) { closeDialog(); }

void onClear(lv_event_t *) {
  s_count = 0;
  memset(s_digits, 0, sizeof(s_digits));
  clearResult();
  renderMask();
}

void onKeyPress(lv_event_t *e) {
  lv_obj_t *bm = lv_event_get_target(e);
  if (!bm) return;
  const char *txt =
      lv_btnmatrix_get_btn_text(bm, lv_btnmatrix_get_selected_btn(bm));
  if (!txt) return;
  if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
    if (s_count > 0) s_digits[--s_count] = '\0';
  } else if (txt[0] >= '0' && txt[0] <= '9' && s_count < DIGIT_COUNT) {
    s_digits[s_count++] = txt[0];
  }
  // Cualquier tecleo invalida el "Enviando..." / "Hora rechazada" anterior:
  // se referia a unos digitos que ya no son los que hay en pantalla.
  clearResult();
  renderMask();
}

uint8_t daysInMonth(int year, int month) {
  static const uint8_t DAYS[12] = {31, 28, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 0;
  if (month == 2) {
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return leap ? 29 : 28;
  }
  return DAYS[month - 1];
}

// Valida la mascara completa antes de mandar nada. El dia se comprueba
// tambien contra la longitud del mes (un 31/02 pasa los rangos por separado y
// no existe), porque de esta hora cuelgan las marcas de tiempo del historial
// clinico.
void onApply(lv_event_t *) {
  if (s_count < DIGIT_COUNT) {
    UI_ShowToast(TR(STR_FILL_DATE_TIME), 2500);
    return;
  }

  const int day = (s_digits[0] - '0') * 10 + (s_digits[1] - '0');
  const int month = (s_digits[2] - '0') * 10 + (s_digits[3] - '0');
  const int year = YEAR_BASE + (s_digits[4] - '0') * 10 + (s_digits[5] - '0');
  const int hour = (s_digits[6] - '0') * 10 + (s_digits[7] - '0');
  const int minute = (s_digits[8] - '0') * 10 + (s_digits[9] - '0');

  if (month < 1 || month > 12) {
    UI_ShowToast(TR(STR_MONTH_OUT_OF_RANGE), 2500);
    return;
  }
  if (year < YEAR_MIN || year > YEAR_MAX) {
    char msg[64];
    snprintf(msg, sizeof(msg), TR(STR_YEAR_OUT_OF_RANGE_FMT),
             YEAR_MIN - YEAR_BASE, YEAR_MAX - YEAR_BASE);
    UI_ShowToast(msg, 2500);
    return;
  }
  const uint8_t maxDay = daysInMonth(year, month);
  if (day < 1 || day > maxDay) {
    char msg[64];
    snprintf(msg, sizeof(msg), TR(STR_DAY_OUT_OF_RANGE_FMT), (unsigned)maxDay);
    UI_ShowToast(msg, 2500);
    return;
  }
  if (hour > 23) {
    UI_ShowToast(TR(STR_HOUR_OUT_OF_RANGE), 2500);
    return;
  }
  if (minute > 59) {
    UI_ShowToast(TR(STR_MINUTE_OUT_OF_RANGE), 2500);
    return;
  }

  Communication_SendSetTime(year, month, day, hour, minute);
  if (s_resultLbl)
    lv_label_set_text(s_resultLbl,
                      TR(STR_SENDING));
}

void buildContent() {
  if (!s_content) return;
  lv_obj_clean(s_content);
  s_maskLbl = nullptr;
  s_resultLbl = nullptr;

  lv_obj_t *title = lv_label_create(s_content);
  lv_label_set_text(title,
                    TR(STR_ADJUST_TIME_UC));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

  // Tiene el aspecto de un textarea pero no lo es: aqui no se teclea texto
  // libre, solo se rellenan los huecos de una mascara fija.
  lv_obj_t *field = lv_obj_create(s_content);
  lv_obj_set_size(field, 360, 56);
  lv_obj_align(field, LV_ALIGN_TOP_MID, 0, 36);
  lv_obj_set_style_radius(field, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_color(field, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_border_color(field, lv_color_hex(0xBBBBBB), LV_PART_MAIN);
  lv_obj_set_style_border_width(field, 2, LV_PART_MAIN);
  lv_obj_clear_flag(field, LV_OBJ_FLAG_SCROLLABLE);

  s_maskLbl = lv_label_create(field);
  lv_obj_set_style_text_font(s_maskLbl, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(s_maskLbl, lv_color_hex(0x0B2E4F), 0);
  lv_obj_center(s_maskLbl);

  lv_obj_t *hint = lv_label_create(s_content);
  lv_label_set_text(
      hint, TR(STR_DATE_TIME_HINT));
  lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 98);

  lv_obj_t *clear = makeBtn(s_content, TR(STR_CLEAR_UC),
                            onClear, lv_color_hex(0x888888));
  lv_obj_set_size(clear, 150, 46);
  lv_obj_align(clear, LV_ALIGN_TOP_LEFT, 6, 126);

  lv_obj_t *apply = makeBtn(s_content, TR(STR_APPLY_UC),
                            onApply, lv_color_hex(0x00AA00));
  lv_obj_set_size(apply, 190, 46);
  lv_obj_align(apply, LV_ALIGN_TOP_RIGHT, -6, 126);

  // Entre los dos botones: ahi cabe sin solaparse con ninguno.
  s_resultLbl = lv_label_create(s_content);
  lv_label_set_text(s_resultLbl, "");
  lv_obj_set_style_text_color(s_resultLbl, lv_color_hex(0x0B2E4F), 0);
  lv_obj_align(s_resultLbl, LV_ALIGN_TOP_MID, 0, 138);

  lv_obj_t *kb = lv_btnmatrix_create(s_content);
  lv_btnmatrix_set_map(kb, KB_DIGITS_MAP);
  lv_btnmatrix_set_ctrl_map(kb, KB_DIGITS_CTRL);
  lv_obj_set_size(kb, 420, 250);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_obj_set_style_text_font(kb, &lv_font_montserrat_20, LV_PART_ITEMS);
  lv_obj_add_event_cb(kb, onKeyPress, LV_EVENT_VALUE_CHANGED, nullptr);

  renderMask();
}

} // namespace

void TimeDialog_Init(lv_obj_t *parent) {
  s_overlay = lv_obj_create(parent);
  lv_obj_remove_style_all(s_overlay);
  lv_obj_set_size(s_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_pos(s_overlay, 0, 0);
  lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_overlay, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(s_overlay, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

  s_card = lv_obj_create(s_overlay);
  lv_obj_set_size(s_card, CARD_W, CARD_H);
  lv_obj_center(s_card);
  lv_obj_set_style_radius(s_card, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(s_card, LV_OBJ_FLAG_SCROLLABLE);

  s_content = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_content);
  lv_obj_set_size(s_content, CARD_W - 20, CARD_H - 20);
  lv_obj_center(s_content);
  lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

  // Despues de s_content a proposito: ultimo hijo = se dibuja encima.
  s_closeBtn = makeBtn(s_card, "X", onClose, lv_color_hex(0xAA3333));
  lv_obj_set_size(s_closeBtn, 46, 46);
  lv_obj_align(s_closeBtn, LV_ALIGN_TOP_RIGHT, 0, 0);
}

void TimeDialog_Open(void) {
  if (!s_overlay) return;
  s_count = 0;
  memset(s_digits, 0, sizeof(s_digits));
  buildContent();
  s_open = true;
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
}

void TimeDialog_Poll(void) {
  // Una alarma critica se lleva la pantalla por delante, igual que hace
  // BabyWizard_Poll() con su wizard.
  if (s_open && UI_IsCriticalAlarmActive()) closeDialog();

  // El ACK se consume este el dialogo abierto o no: dejarlo pendiente lo
  // haria saltar en la siguiente apertura, ya sin relacion con lo que el
  // operador acabase de teclear.
  if (!g_pendingTimeAck) return;
  g_pendingTimeAck = false;

  const bool accepted = (g_timeAckResult == 0);
  const char *msg =
      accepted ? TR(STR_TIME_SET)
               : TR(STR_TIME_REJECTED);
  UI_ShowToast(msg, 3000);

  if (!s_open) return;
  if (accepted) {
    closeDialog();
  } else {
    // Rechazada: se deja abierto y con lo tecleado, que es lo que hay que
    // corregir.
    if (s_resultLbl) lv_label_set_text(s_resultLbl, msg);
  }
}

#include "ui/BabyWizard.h"

#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "UITask.h"
#include "main.h"
#include "ui.h"

// --- Shared state owned by UITask.cpp (same pattern CommTask.cpp uses) ---
extern ui_lang_t g_lang;

namespace {

enum class WizStep {
  Closed,
  RequestingList,
  ChooseBaby,
  EnterName,
  EnterGest,
  WaitingNewAck,
  WaitingSelectAck,
  EnterWeight,
  WaitingRange,
  EnterAge,
  WaitingRange2,
  Summary,
};

constexpr uint32_t LIST_TIMEOUT_MS = 2000;
constexpr uint32_t ACK_TIMEOUT_MS = 3000;
constexpr uint32_t RANGE_TIMEOUT_MS = 3000;
constexpr size_t BABY_NAME_LEN_LOCAL = 24; // matches motherBoard BABY_NAME_LEN

// What the wizard is gating. AIR/SKIN pick a thermal control mode and care
// about the NTE range; PHOTOTHERAPY and HUMIDITY only collect the baby data
// (no NTE range applies to a lamp or a humidifier) and hand off to their own
// activation path.
enum class WizTarget : uint8_t { Air, Skin, Phototherapy, Humidity };

// True for the two targets that identify the baby and stop there: no weight,
// no age-in-days, no range proposal, no summary screen.
bool targetIsIdentityOnly(WizTarget t) {
  return t == WizTarget::Phototherapy || t == WizTarget::Humidity;
}

WizStep s_step = WizStep::Closed;
WizTarget s_target = WizTarget::Air;
// Kept as a derived convenience: every existing check reads "is this the
// air-temperature flow?", which stays false for both SKIN and PHOTOTHERAPY.
bool s_desiredIsAir = true;
uint32_t s_seq = 0;
char s_name[BABY_NAME_LEN_LOCAL] = "";
// 0 = not provided yet. No clinical value is ever invented as a default:
// every field is either typed by the user or recalled from a stored profile.
uint8_t s_gestWeeks = 0;
uint16_t s_weightGrams = 0;
uint16_t s_ageDays = 0;
bool s_rangeAgeKnown = false;
float s_rangeLo = -1.0f, s_rangeHi = -1.0f, s_rangeMid = -1.0f;
bool s_rangeEstimated = true;
bool s_hasUsableRange = false;
uint32_t s_deadlineMs = 0;
int s_listRetries = 0;
BabyProfileListMsg s_list = {0, {}};

// Who is in the incubator right now, as far as this HMI knows. Outlives an
// individual wizard run (unlike s_seq, which is per-run) so a second therapy
// does not re-ask which baby it is. Cleared only on discharge.
uint32_t s_sessionSeq = 0;
char s_sessionName[BABY_NAME_LEN_LOCAL] = "";
uint8_t s_sessionGest = 0;
uint16_t s_sessionWeight = 0;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_card = nullptr;
lv_obj_t *s_content = nullptr;
// Always-visible cancel affordance. Deliberately a child of s_card and not of
// s_content, so clearContent() between steps never destroys it: every screen
// of the wizard, including the ones with no BACK, can always be abandoned.
lv_obj_t *s_closeBtn = nullptr;
lv_obj_t *s_keyboard = nullptr;
lv_obj_t *s_nameTa = nullptr;
// Single numeric textarea reused by the gest-weeks / weight / age-days steps
// (one input per screen).
lv_obj_t *s_inputTa = nullptr;

// --- Card geometry -----------------------------------------------------
// Keyboard steps take over almost the whole screen so the keys stay big;
// list/summary steps keep the smaller dialog look.
constexpr lv_coord_t CARD_W_SMALL = 640, CARD_H_SMALL = 420;
constexpr lv_coord_t CARD_W_BIG = 780, CARD_H_BIG = 460;

// --- Custom keymaps ----------------------------------------------------
// Letters only, uppercase: no digits, and crucially no ',' key at all —
// the protocol is comma-delimited, so the character is unreachable by
// construction here (onNameChanged stays as defense in depth).
const char *KB_LETTERS_MAP[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    " ", ""};
// 28 buttons: 10 + 9 + 8 + 1. Must match KB_LETTERS_MAP exactly.
const lv_btnmatrix_ctrl_t KB_LETTERS_CTRL[28] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1};

// Digits only: no decimal point, no sign, no mode-switch key.
const char *KB_DIGITS_MAP[] = {"1", "2", "3", "\n",
                               "4", "5", "6", "\n",
                               "7", "8", "9", "\n",
                               LV_SYMBOL_BACKSPACE, "0", ""};
// 11 buttons: 3 + 3 + 3 + 2.
const lv_btnmatrix_ctrl_t KB_DIGITS_CTRL[11] = {1, 1, 1, 1, 1, 1,
                                                1, 1, 1, 1, 1};

const char *TXT(const char *es, const char *en, const char *fr) {
  return (g_lang == LANG_ES) ? es : (g_lang == LANG_FR) ? fr : en;
}

void setCardSize(bool big) {
  lv_coord_t w = big ? CARD_W_BIG : CARD_W_SMALL;
  lv_coord_t h = big ? CARD_H_BIG : CARD_H_SMALL;
  if (s_card) {
    lv_obj_set_size(s_card, w, h);
    lv_obj_center(s_card);
  }
  if (s_content) {
    lv_obj_set_size(s_content, w - 20, h - 20);
    lv_obj_center(s_content);
  }
}

void clearContent() {
  if (s_content) lv_obj_clean(s_content);
  s_keyboard = nullptr;
  s_nameTa = nullptr;
  s_inputTa = nullptr;
}

lv_obj_t *makeTitle(const char *text) {
  lv_obj_t *lbl = lv_label_create(s_content);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 6);
  return lbl;
}

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

void showLoadingScreen();
void showChooseBabyScreen();
void showNameScreen();
void showGestScreen();
void showWeightScreen();
void showAgeScreen();
void showSummaryScreen();
void closeOverlay();
void cancelWizard();
void finishWizard(bool useRange);
void onSkipClicked(lv_event_t *e);

// Key handler for the on-screen keypads (see buildInputStep for why these
// are button matrices rather than lv_keyboard widgets).
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

// --- Shared builder for the single-input steps -------------------------
// Every input step looks the same: title, one input, a range hint, BACK /
// CONTINUE, and a full-width keyboard underneath. `digits` picks the
// keymap (digits-only vs letters-only) so the keyboard always matches what
// the step is asking for.
lv_obj_t *buildInputStep(const char *title, const char *hint, bool digits,
                         lv_event_cb_t onBack, lv_event_cb_t onContinue,
                         const char *backTxt = nullptr) {
  clearContent();
  setCardSize(true);
  makeTitle(title);

  lv_obj_t *ta = lv_textarea_create(s_content);
  lv_obj_set_size(ta, digits ? 220 : 520, 52);
  lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 40);
  lv_textarea_set_one_line(ta, true);
  lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_align(ta, LV_TEXT_ALIGN_CENTER, 0);
  if (digits) {
    lv_textarea_set_accepted_chars(ta, "0123456789");
    lv_textarea_set_max_length(ta, 5);
  } else {
    lv_textarea_set_max_length(ta, BABY_NAME_LEN_LOCAL - 1);
  }
  lv_textarea_set_placeholder_text(ta, "...");

  lv_obj_t *hintLbl = lv_label_create(s_content);
  lv_label_set_text(hintLbl, hint);
  lv_obj_set_style_text_color(hintLbl, lv_color_hex(0x666666), 0);
  lv_obj_align(hintLbl, LV_ALIGN_TOP_MID, 0, 98);

  lv_obj_t *back = makeBtn(s_content, backTxt ? backTxt
                                              : TXT("ATRAS", "BACK", "RETOUR"),
                          onBack, lv_color_hex(0x888888));
  lv_obj_set_size(back, 150, 46);
  lv_obj_align(back, LV_ALIGN_TOP_LEFT, 6, 126);

  lv_obj_t *cont = makeBtn(s_content, TXT("CONTINUAR", "CONTINUE", "CONTINUER"),
                          onContinue, lv_color_hex(0x00AA00));
  lv_obj_set_size(cont, 190, 46);
  lv_obj_align(cont, LV_ALIGN_TOP_RIGHT, -6, 126);

  // SKIP on every step: abandon the wizard and run in manual (amber, so it
  // reads as "escape hatch" rather than as the normal green path).
  lv_obj_t *skip = makeBtn(s_content, TXT("SALTAR", "SKIP", "PASSER"),
                          onSkipClicked, lv_color_hex(0xE08800));
  lv_obj_set_size(skip, 160, 46);
  lv_obj_align(skip, LV_ALIGN_TOP_MID, 0, 126);

  // Deliberately an lv_btnmatrix, NOT an lv_keyboard: LVGL 8.3 stores
  // keyboard keymaps in a file-static global (`kb_map[mode]`), so
  // lv_keyboard_set_map() would rewrite the map for *every* keyboard in the
  // app — including ui_Keyboard1, the persistent WiFi-credentials keyboard,
  // which would lose its digits. lv_btnmatrix keeps the map per instance.
  s_keyboard = lv_btnmatrix_create(s_content);
  lv_btnmatrix_set_map(s_keyboard, digits ? KB_DIGITS_MAP : KB_LETTERS_MAP);
  lv_btnmatrix_set_ctrl_map(s_keyboard,
                            digits ? KB_DIGITS_CTRL : KB_LETTERS_CTRL);
  lv_obj_set_size(s_keyboard, digits ? 420 : 750, 250);
  lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_obj_set_style_text_font(s_keyboard, &lv_font_montserrat_20,
                             LV_PART_ITEMS);
  lv_obj_add_event_cb(s_keyboard, onKeyPress, LV_EVENT_VALUE_CHANGED, ta);
  return ta;
}

// Reads the numeric textarea; false when empty or outside [lo, hi].
bool readNumericInput(uint32_t lo, uint32_t hi, uint32_t *out) {
  if (!s_inputTa) return false;
  const char *txt = lv_textarea_get_text(s_inputTa);
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

void showRangeError(uint32_t lo, uint32_t hi) {
  char es[64], en[64], fr[64];
  snprintf(es, sizeof(es), "Introduce un valor entre %u y %u", (unsigned)lo,
           (unsigned)hi);
  snprintf(en, sizeof(en), "Enter a value between %u and %u", (unsigned)lo,
           (unsigned)hi);
  snprintf(fr, sizeof(fr), "Entrez une valeur entre %u et %u", (unsigned)lo,
           (unsigned)hi);
  UI_ShowToast(TXT(es, en, fr), 2500);
}

void closeOverlay() {
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  clearContent();
}

void cancelWizard() {
  // Only revert the switch this wizard was opened for. Phototherapy and
  // humidity come in on ui_Switch3/ui_Switch2 (both already left OFF by their
  // handler before opening us), so touching ui_Switch1 here would silently
  // kill a running thermal control just because the nurse backed out of one
  // of those dialogs.
  if (!targetIsIdentityOnly(s_target)) {
    ui_set_switch_state_silent(ui_Switch1, false);
  }
  closeOverlay();
  s_step = WizStep::Closed;
}

void onCancelClicked(lv_event_t *) { cancelWizard(); }

// Built once, in BabyWizard_Init, right after s_content: being the card's last
// child it always draws over the step's own widgets, and it survives every
// clearContent(). setCardSize() re-lays out the card, and LVGL re-applies the
// stored alignment, so the X follows the small/big card without extra work.
void buildCloseButton() {
  if (!s_card || s_closeBtn) return;
  s_closeBtn = makeBtn(s_card, "X", onCancelClicked, lv_color_hex(0xAA3333));
  lv_obj_set_size(s_closeBtn, 46, 46);
  lv_obj_align(s_closeBtn, LV_ALIGN_TOP_RIGHT, 0, 0);
}

void showLoadingScreen() {
  clearContent();
  setCardSize(false);
  makeTitle(TXT("Cargando bebes...", "Loading babies...",
                "Chargement des bebes..."));
}

void selectExisting(uint32_t seq, uint8_t gest, uint16_t lastWeight) {
  s_seq = seq;
  s_gestWeeks = gest;
  s_weightGrams = lastWeight; // 0 = never recorded -> weight step starts empty
  Communication_SendProfileSelect(seq);
  s_step = WizStep::WaitingSelectAck;
  s_deadlineMs = millis() + ACK_TIMEOUT_MS;
  showLoadingScreen();
}

void onNewBabyClicked(lv_event_t *) {
  s_name[0] = '\0';
  showNameScreen();
  s_step = WizStep::EnterName;
}

void showChooseBabyScreen() {
  clearContent();
  setCardSize(false);
  makeTitle(TXT("Bebe nuevo o existente", "New or existing baby",
                "Bebe nouveau ou existant"));

  int y = 60;
  for (int i = 0; i < s_list.count; i++) {
    const BabyProfileListItem &it = s_list.items[i];
    lv_obj_t *card = lv_btn_create(s_content);
    lv_obj_set_size(card, 560, 60);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y);
    // lv_btn's default theme paints its label white; on the old light-grey
    // fill that left white-on-grey. Tinted fill + blue border + explicit
    // dark text instead, so the row is both readable and obviously tappable.
    lv_obj_set_style_bg_color(card, lv_color_hex(0xE3F0FF), LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0x0075EE), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xBBD9F7),
                              LV_PART_MAIN | LV_STATE_PRESSED);

    char buf[80];
    if (it.weightGrams > 0) {
      snprintf(buf, sizeof(buf), "%s  -  EG %u sem  -  %u g", it.name,
               (unsigned)it.gestWeeks, (unsigned)it.weightGrams);
    } else {
      snprintf(buf, sizeof(buf), "%s  -  EG %u sem  -  --", it.name,
               (unsigned)it.gestWeeks);
    }
    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x0B2E4F), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl);

    // Bind by value via user_data (seq/gest/weight packed into a static
    // per-row struct — simplest correct capture without lambdas needing
    // heap state LVGL doesn't manage for us).
    struct RowData { uint32_t seq; uint8_t gest; uint16_t w; };
    static RowData rows[3];
    rows[i] = {it.seq, it.gestWeeks, it.weightGrams};
    lv_obj_add_event_cb(
        card,
        [](lv_event_t *e) {
          auto *rd = (RowData *)lv_event_get_user_data(e);
          selectExisting(rd->seq, rd->gest, rd->w);
        },
        LV_EVENT_CLICKED, &rows[i]);
    y += 70;
  }

  lv_obj_t *newBtn = makeBtn(s_content, TXT("BEBE NUEVO", "NEW BABY", "NOUVEAU BEBE"),
                            onNewBabyClicked, lv_color_hex(0x0075EE));
  lv_obj_set_size(newBtn, 300, 56);
  lv_obj_align(newBtn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

  // Same escape hatch as the input steps: skip all baby data, run manual.
  lv_obj_t *skip = makeBtn(s_content, TXT("SALTAR", "SKIP", "PASSER"),
                          onSkipClicked, lv_color_hex(0xE08800));
  lv_obj_set_size(skip, 180, 56);
  lv_obj_align(skip, LV_ALIGN_BOTTOM_LEFT, 10, -10);
}

void onNameChanged(lv_event_t *e) {
  lv_obj_t *ta = lv_event_get_target(e);
  const char *txt = lv_textarea_get_text(ta);
  // Defense in depth: the letters keymap has no comma key, but a stray
  // comma must never reach the comma-delimited protocol either way.
  if (!strchr(txt, ',')) return;
  char clean[BABY_NAME_LEN_LOCAL];
  size_t j = 0;
  for (size_t i = 0; txt[i] && j < sizeof(clean) - 1; i++) {
    if (txt[i] != ',') clean[j++] = txt[i];
  }
  clean[j] = '\0';
  lv_textarea_set_text(ta, clean);
}

// ---------------- Step: name (letters-only keyboard) ----------------

void onNameBack(lv_event_t *) {
  showChooseBabyScreen();
  s_step = WizStep::ChooseBaby;
}

void onNameContinue(lv_event_t *) {
  const char *txt = s_nameTa ? lv_textarea_get_text(s_nameTa) : "";
  if (!txt || txt[0] == '\0') {
    UI_ShowToast(TXT("Introduce un nombre", "Enter a name", "Entrez un nom"),
                2500);
    return;
  }
  snprintf(s_name, sizeof(s_name), "%s", txt);
  showGestScreen();
  s_step = WizStep::EnterGest;
}

void showNameScreen() {
  s_nameTa = buildInputStep(
      TXT("Nombre del bebe", "Baby name", "Nom du bebe"),
      TXT("Solo letras", "Letters only", "Lettres uniquement"), false,
      onNameBack, onNameContinue);
  lv_obj_add_event_cb(s_nameTa, onNameChanged, LV_EVENT_VALUE_CHANGED,
                      nullptr);
  if (s_name[0] != '\0') lv_textarea_set_text(s_nameTa, s_name);
}

// ---------------- Step: gestational age (numeric keypad) ----------------

void onGestBack(lv_event_t *) {
  showNameScreen();
  s_step = WizStep::EnterName;
}

void onGestContinue(lv_event_t *) {
  uint32_t v = 0;
  if (!readNumericInput(20, 40, &v)) {
    showRangeError(20, 40);
    return;
  }
  s_gestWeeks = (uint8_t)v;
  Communication_SendProfileNew(s_name, s_gestWeeks);
  s_step = WizStep::WaitingNewAck;
  s_deadlineMs = millis() + ACK_TIMEOUT_MS;
  showLoadingScreen();
}

void showGestScreen() {
  s_inputTa = buildInputStep(
      TXT("Edad gestacional (semanas)", "Gestational age (weeks)",
          "Age gestationnel (semaines)"),
      "20 - 40", true, onGestBack, onGestContinue);
  // Left empty on purpose: a prefilled number invites confirming someone
  // else's value by reflex. The nurse must type the real one.
}

// ---------------- Step: weight (numeric keypad + SKIP) ----------------

void sendWeightAndWait(uint16_t grams) {
  Communication_SendProfileWeight(s_seq, grams);
  s_step = WizStep::WaitingRange;
  s_deadlineMs = millis() + RANGE_TIMEOUT_MS;
  showLoadingScreen();
}

void onWeightSkip(lv_event_t *) { sendWeightAndWait(0); }

void onWeightContinue(lv_event_t *) {
  uint32_t v = 0;
  if (!readNumericInput(400, 5000, &v)) {
    showRangeError(400, 5000);
    return;
  }
  s_weightGrams = (uint16_t)v;
  sendWeightAndWait(s_weightGrams);
}

void showWeightScreen() {
  // Two distinct escapes here, deliberately labelled apart:
  //   "SIN PESO" (this back slot) stays in the wizard and tells the board the
  //   weight is unknown, so the profile is still completed and recorded.
  //   "SALTAR" (the shared SKIP from buildInputStep) abandons the wizard
  //   altogether and drops straight to manual control.
  // The step is reached right after the board accepted the profile, so there
  // is no earlier screen to go back to and the slot is free for this.
  s_inputTa = buildInputStep(
      TXT("Peso actual (gramos)", "Current weight (grams)",
          "Poids actuel (grammes)"),
      "400 - 5000 g", true, onWeightSkip, onWeightContinue,
      TXT("SIN PESO", "NO WEIGHT", "SANS POIDS"));
  // Deliberately empty, aunque exista un ultimo peso conocido (s_weightGrams).
  //
  // Prefijarlo invitaba a confirmar sin pesar: al reactivar el control de un
  // bebe ya existente, el cuadro aparecia con el peso de la ULTIMA vez —a
  // veces del dia anterior— y bastaba con pulsar "continuar" sin tocarlo para
  // que quedase registrado como si fuera la medida de hoy. Un peso vacio
  // obliga a teclear uno real o a pulsar "SIN PESO" explicitamente; ninguna
  // de las dos deja un dato de ayer pasando por el de hoy.
}

// ---------------- Step: age in days (numeric keypad) ----------------

void onAgeContinue(lv_event_t *) {
  uint32_t v = 0;
  if (!readNumericInput(0, 365, &v)) {
    showRangeError(0, 365);
    return;
  }
  s_ageDays = (uint16_t)v;
  Communication_SendProfileAgeManual(s_seq, s_ageDays);
  s_step = WizStep::WaitingRange2;
  s_deadlineMs = millis() + RANGE_TIMEOUT_MS;
  showLoadingScreen();
}

void onAgeBack(lv_event_t *) {
  showWeightScreen();
  s_step = WizStep::EnterWeight;
}

void showAgeScreen() {
  s_inputTa = buildInputStep(
      TXT("Edad en dias (sin hora sincronizada)",
          "Age in days (no synced time)", "Age en jours (heure non sync.)"),
      "0 - 365", true, onAgeBack, onAgeContinue);
  // No prefill: an age of 0 days is a real, clinically meaningful value,
  // so it must be typed rather than accepted by default.
}

// Single exit point for the whole wizard. `useRange` is false for every
// SKIP path (activate in plain manual, no NTE proposal) and true only when
// Apply is pressed on a summary that actually has a range.
//
// Factored out so the five SKIP buttons, the list-screen SKIP and Apply all
// reach control activation through one place. A missing range no longer stops
// SKIN: its 36.5 degC setpoint is fixed by the clinical standard and does not
// come from the NTE bounds, so what SKIP costs there is the proposed AIR
// temperature, not the therapy. The only precondition SKIN really has is a
// connected probe, and that one is checked (and reported) by ui_Switch4.
void finishWizard(bool useRange) {
  bool haveRange = useRange && !s_rangeEstimated && s_rangeLo >= 0.0f;

  if (targetIsIdentityOnly(s_target)) {
    // No thermal range involved; just let the lamp / humidifier path continue.
    bool wasPhoto = (s_target == WizTarget::Phototherapy);
    closeOverlay();
    s_step = WizStep::Closed;
    if (wasPhoto) {
      ActivatePhototherapyFromWizard();
    } else {
      ActivateHumidityFromWizard();
    }
    return;
  }

  if (s_target == WizTarget::Skin) {
    s_hasUsableRange = haveRange;
    if (!haveRange) {
      // Se activa igual: 36.5 degC de consigna de piel no salen del rango NTE,
      // asi que sin peso lo unico que falta es la propuesta de temperatura de
      // AIRE. Se avisa de lo que no hay, pero no se bloquea la terapia.
      UI_ShowToast(TXT("Modo piel activo sin rango automatico (sin peso)",
                       "Skin mode active, no automatic range (no weight)",
                       "Mode peau actif, sans plage automatique (sans poids)"),
                   4000);
    }
    ActivateTempControlUI(false);
    ui_set_switch_state_silent(ui_Switch4, true);
    lv_event_send(ui_Switch4, LV_EVENT_VALUE_CHANGED, nullptr);
  } else {
    if (haveRange) airTempValue = s_rangeMid;
    ActivateTempControlUI(true);
  }
  closeOverlay();
  s_step = WizStep::Closed;
}

void onApplyClicked(lv_event_t *) { finishWizard(true); }

// SKIP, available on every step: abandon data collection and go straight to
// manual control.
void onSkipClicked(lv_event_t *) { finishWizard(false); }

void showSummaryScreen() {
  clearContent();
  setCardSize(true);
  makeTitle(TXT("Resumen", "Summary", "Resume"));

  bool haveRange = !s_rangeEstimated && s_rangeLo >= 0.0f;
  // Only worth showing the placement guide when there is actually a probe to
  // place. Anything other than NOT_CONNECTED counts as present: a probe that
  // is pending validation, unstable or out of range is still physically
  // there, and those are exactly the cases where bad placement is the likely
  // cause, so the guide is most useful precisely then.
  bool probePresent = (g_skinProbeState != SKIN_PROBE_NOT_CONNECTED);

  lv_coord_t infoTop = 44;

  if (probePresent) {
    lv_obj_t *img = lv_img_create(s_content);
    lv_img_set_src(img, &ui_img_baby_place_sensor_png);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *imgCap = lv_label_create(s_content);
    lv_label_set_text(imgCap, TXT("Coloca el sensor de piel asi",
                                  "Place the skin sensor like this",
                                  "Placez le capteur cutane ainsi"));
    lv_obj_set_style_text_color(imgCap, lv_color_hex(0x666666), 0);
    lv_obj_align(imgCap, LV_ALIGN_TOP_MID, 0, 274);
    infoTop = 296;  // below the 230 px image + its caption
  }

  if (haveRange) {
    // Range panel: the midpoint is what actually gets applied, so it is the
    // biggest element; the bounds frame it. Laid out as one wide strip so it
    // fits under the image when the guide is shown.
    lv_obj_t *panel = lv_obj_create(s_content);
    lv_obj_set_size(panel, 620, probePresent ? 86 : 200);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, infoTop);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xE3F0FF), LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x0075EE), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 10, LV_PART_MAIN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    char buf[48];
    snprintf(buf, sizeof(buf), "%.1f C", (double)s_rangeMid);
    lv_obj_t *mid = lv_label_create(panel);
    lv_label_set_text(mid, buf);
    lv_obj_set_style_text_font(mid, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(mid, lv_color_hex(0x0075EE), 0);
    lv_obj_align(mid, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t *cap = lv_label_create(panel);
    lv_label_set_text(cap, TXT("Temperatura de aire propuesta",
                               "Proposed air temperature",
                               "Temperature d'air proposee"));
    lv_obj_set_style_text_color(cap, lv_color_hex(0x0B2E4F), 0);
    lv_obj_align(cap, LV_ALIGN_RIGHT_MID, -12, -14);

    snprintf(buf, sizeof(buf), TXT("rango %.1f - %.1f C",
                                   "range %.1f - %.1f C",
                                   "plage %.1f - %.1f C"),
             (double)s_rangeLo, (double)s_rangeHi);
    lv_obj_t *rng = lv_label_create(panel);
    lv_label_set_text(rng, buf);
    lv_obj_set_style_text_color(rng, lv_color_hex(0x0B2E4F), 0);
    lv_obj_align(rng, LV_ALIGN_RIGHT_MID, -12, 14);
  } else {
    lv_obj_t *info = lv_label_create(s_content);
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info, 700);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, infoTop + 10);
    lv_label_set_text(
        info,
        s_target == WizTarget::Skin
            ? TXT("Sin rango automatico: el modo PIEL no se puede "
                  "activar.",
                  "No automatic range: SKIN mode cannot be activated.",
                  "Pas de plage automatique : le mode PEAU ne peut pas "
                  "etre active.")
            : TXT("Sin rango automatico (peso no informado).\nEl modo "
                  "AIRE arrancara en manual.",
                  "No automatic range (weight not provided).\nAIR mode "
                  "will start in manual.",
                  "Pas de plage automatique (poids non fourni).\nLe "
                  "mode AIR demarrera en manuel."));
  }

  lv_obj_t *cancel =
      makeBtn(s_content, TXT("CANCELAR", "CANCEL", "ANNULER"), onCancelClicked,
             lv_color_hex(0x888888));
  lv_obj_set_size(cancel, 180, 48);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 10, -10);

  lv_obj_t *skip = makeBtn(s_content, TXT("SALTAR", "SKIP", "PASSER"),
                          onSkipClicked, lv_color_hex(0xE08800));
  lv_obj_set_size(skip, 160, 48);
  lv_obj_align(skip, LV_ALIGN_BOTTOM_MID, 0, -10);

  bool applyDisabled = (s_target == WizTarget::Skin) && !haveRange;
  lv_obj_t *apply =
      makeBtn(s_content, TXT("APLICAR", "APPLY", "APPLIQUER"), onApplyClicked,
             applyDisabled ? lv_color_hex(0xAAAAAA) : lv_color_hex(0x00AA00));
  lv_obj_set_size(apply, 180, 48);
  lv_obj_align(apply, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  if (applyDisabled) {
    lv_obj_clear_flag(apply, LV_OBJ_FLAG_CLICKABLE);
  }
}

} // namespace

void BabyWizard_Init(lv_obj_t *parent) {
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
  lv_obj_set_size(s_card, 640, 420);
  lv_obj_center(s_card);
  lv_obj_set_style_radius(s_card, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(s_card, LV_OBJ_FLAG_SCROLLABLE);

  s_content = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_content);
  lv_obj_set_size(s_content, 620, 400);
  lv_obj_center(s_content);
  lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

  // After s_content on purpose: last child = drawn on top of every step.
  buildCloseButton();
}

static void openForTarget(WizTarget target) {
  s_target = target;
  s_desiredIsAir = (target == WizTarget::Air);
  s_seq = 0;
  s_gestWeeks = 0;
  s_weightGrams = 0;
  s_ageDays = 0;
  s_hasUsableRange = false;
  s_listRetries = 0;

  if (BabyWizard_HasLiveSession()) {
    // Baby already identified AND still under care right now (e.g.
    // phototherapy is running and temperature is being added): skip the
    // picker and go straight to whatever this therapy still needs.
    // PROFILE_SELECT is re-sent rather than assumed, so the motherBoard's
    // wizard state is re-synced instead of trusted.
    //
    // The live-therapy check is what makes the shortcut safe: once the
    // incubator goes fully idle the care session is over, so the next
    // activation must identify the baby again even though the profile is
    // still remembered (the exit dialog's answer — kangaroo, "not now", or
    // no dialog at all because an alarm owned the screen — decides the
    // clinical record, never who the picker offers).
    selectExisting(s_sessionSeq, s_sessionGest, s_sessionWeight);
  } else {
    Communication_SendProfileListReq();
    s_step = WizStep::RequestingList;
    s_deadlineMs = millis() + LIST_TIMEOUT_MS;
    showLoadingScreen();
  }

  if (s_overlay) {
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
  }
}

void BabyWizard_Open(bool desiredIsAirMode) {
  openForTarget(desiredIsAirMode ? WizTarget::Air : WizTarget::Skin);
}

void BabyWizard_OpenForPhototherapy() {
  openForTarget(WizTarget::Phototherapy);
}

void BabyWizard_OpenForHumidity() { openForTarget(WizTarget::Humidity); }

bool BabyWizard_HasUsableRange() { return s_hasUsableRange; }

uint32_t BabyWizard_GetActiveSeq() { return s_sessionSeq; }
bool BabyWizard_HasLiveSession() {
  return s_sessionSeq != 0 && UI_AnyControlActive();
}
const char *BabyWizard_GetActiveName() { return s_sessionName; }
void BabyWizard_ClearActiveProfile() {
  s_sessionSeq = 0;
  s_sessionName[0] = '\0';
  s_sessionGest = 0;
  s_sessionWeight = 0;
  s_seq = 0;
  s_name[0] = '\0';
  s_hasUsableRange = false;
}

BabyWizardStep BabyWizard_GetStep() {
  switch (s_step) {
    case WizStep::Closed: return BW_CLOSED;
    case WizStep::RequestingList:
    case WizStep::ChooseBaby: return BW_PICKER;
    case WizStep::EnterName:
    case WizStep::EnterGest:
    case WizStep::WaitingNewAck:
    case WizStep::WaitingSelectAck: return BW_IDENTITY;
    case WizStep::EnterWeight:
    case WizStep::WaitingRange: return BW_WEIGHT;
    case WizStep::EnterAge:
    case WizStep::WaitingRange2: return BW_AGE;
    case WizStep::Summary: return BW_SUMMARY;
  }
  return BW_CLOSED;
}

bool BabyWizard_IsOpen() { return s_step != WizStep::Closed; }

void BabyWizard_Cancel() {
  if (s_step != WizStep::Closed) cancelWizard();
}

void BabyWizard_Poll() {
  if (s_step == WizStep::Closed) return;

  if (UI_IsCriticalAlarmActive()) {
    cancelWizard();
    return;
  }

  switch (s_step) {
    case WizStep::RequestingList:
      if (g_pendingProfileList) {
        g_pendingProfileList = false;
        s_list = g_profileList;
        showChooseBabyScreen();
        s_step = WizStep::ChooseBaby;
      } else if (millis() > s_deadlineMs) {
        if (s_listRetries == 0) {
          s_listRetries++;
          Communication_SendProfileListReq();
          s_deadlineMs = millis() + LIST_TIMEOUT_MS;
        } else {
          UI_ShowToast(TXT("No se pudo consultar el historial",
                           "Could not fetch baby history",
                           "Impossible de recuperer l'historique"),
                      3000);
          s_list.count = 0;
          showChooseBabyScreen();
          s_step = WizStep::ChooseBaby;
        }
      }
      break;

    case WizStep::WaitingNewAck:
    case WizStep::WaitingSelectAck:
      if (g_pendingProfileAck) {
        g_pendingProfileAck = false;
        if (g_profileAck == 0) {
          // The remembered baby no longer exists on the motherBoard (it was
          // discharged, FIFO-evicted, or the records were wiped). Forget it
          // so the picker takes over instead of retrying a dead seq forever.
          s_sessionSeq = 0;
          s_sessionName[0] = '\0';
          UI_ShowToast(TXT("Operacion rechazada, intentelo de nuevo",
                           "Operation refused, try again",
                           "Operation refusee, reessayez"),
                      3000);
          // Re-request the list: we may have arrived here bypassing it.
          Communication_SendProfileListReq();
          s_listRetries = 0;
          s_step = WizStep::RequestingList;
          s_deadlineMs = millis() + LIST_TIMEOUT_MS;
          showLoadingScreen();
        } else {
          s_seq = g_profileAck;
          s_sessionSeq = s_seq;
          snprintf(s_sessionName, sizeof(s_sessionName), "%s", s_name);
          s_sessionGest = s_gestWeeks;
          if (s_weightGrams > 0) s_sessionWeight = s_weightGrams;
          if (targetIsIdentityOnly(s_target)) {
            // A lamp or a humidifier needs no NTE range, so weight and
            // age-in-days are never asked here: identifying the baby is the
            // entire point.
            finishWizard(false);
          } else {
            showWeightScreen();
            s_step = WizStep::EnterWeight;
          }
        }
      } else if (millis() > s_deadlineMs) {
        UI_ShowToast(TXT("Sin respuesta de la placa", "No response from board",
                         "Pas de reponse de la carte"),
                    3000);
        showChooseBabyScreen();
        s_step = WizStep::ChooseBaby;
      }
      break;

    case WizStep::WaitingRange:
    case WizStep::WaitingRange2:
      if (g_pendingProfileRange) {
        g_pendingProfileRange = false;
        if (g_profileRange.seq != s_seq) break; // stale response
        s_rangeAgeKnown = g_profileRange.ageKnown;
        s_rangeLo = g_profileRange.lo;
        s_rangeHi = g_profileRange.hi;
        s_rangeMid = g_profileRange.mid;
        s_rangeEstimated = g_profileRange.estimated;
        if (!s_rangeAgeKnown) {
          showAgeScreen();
          s_step = WizStep::EnterAge;
        } else {
          s_hasUsableRange = !s_rangeEstimated && s_rangeLo >= 0.0f;
          showSummaryScreen();
          s_step = WizStep::Summary;
        }
      } else if (millis() > s_deadlineMs) {
        UI_ShowToast(TXT("Sin respuesta de la placa", "No response from board",
                         "Pas de reponse de la carte"),
                    3000);
        showWeightScreen();
        s_step = WizStep::EnterWeight;
      }
      break;

    default:
      break; // pure-UI steps wait for button callbacks
  }
}

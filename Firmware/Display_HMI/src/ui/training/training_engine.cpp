#include "ui/training/training.h"

#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "UITask.h"
#include "main.h"
#include "state/training_mode.h"
#include "ui.h"
#include "ui/AlarmCenter.h"
#include "ui/BabyExitDialog.h"
#include "ui/BabyHistory.h"
#include "ui/BabyWizard.h"
#include "ui/HelpDialog.h"  // HELP_IDLE_TIMEOUT_MS
#include "ui/TelemetryHistory.h"
#include "ui/TimeDialog.h"

// --- Shared state owned by UITask.cpp (same pattern TimeDialog.cpp uses) ---
extern ui_lang_t g_lang;

// Mismo criterio de reserva que UI_Str() con el catalogo: si falta la
// traduccion de un idioma, ingles antes que una pantalla en blanco.
const char *TrainingTxt(const LessonTxt &t) {
  const char *s = nullptr;
  switch (g_lang) {
    case LANG_ES: s = t.es; break;
    case LANG_FR: s = t.fr; break;
    case LANG_PT: s = t.pt; break;
    default:      s = t.en; break;
  }
  if (s == nullptr || s[0] == '\0') s = t.en;
  return (s != nullptr) ? s : "";
}

namespace {

// Como corre la leccion en curso.
enum Mode {
  MODE_NONE = 0,
  MODE_INTERACTIVE,  // modo formacion activo; cuenta como superada
  MODE_PASSIVE,      // leccion sin LESSON_INTERACTIVE; cuenta como superada
  MODE_DEMO,         // interactiva con gate fallido: solo explicar; no cuenta
};

// Geometria (heredada de HelpTour.cpp).
constexpr lv_coord_t BUBBLE_W = 560, BUBBLE_H = 200;
constexpr lv_coord_t BUBBLE_SIDE_W = 400, BUBBLE_SIDE_H = 280;
constexpr lv_coord_t QUIZ_W = 620, QUIZ_H = 360;
constexpr lv_coord_t FRAME_PAD = 6;
constexpr lv_coord_t MARGIN = 16;
constexpr lv_coord_t BTN_H = 44;
constexpr lv_coord_t TALL_TARGET = 240;
// Franja inferior: MODO FORMACION o la instruccion de un paso libre. 32 px en
// el borde inferior, zona libre en todas las pantallas.
constexpr lv_coord_t STRIP_H = 32;
constexpr lv_coord_t STRIP_Y = DISPLAY_HEIGHT - STRIP_H;

const Course *s_course = nullptr;
const Lesson *s_lesson = nullptr;
uint8_t s_lessonIdx = 0;
int s_idx = 0;
Mode s_mode = MODE_NONE;
bool s_open = false;
uint16_t s_attempts = 0;
bool s_quizSolved = false;
UiControlSnapshot s_snap;
// Verdadero mientras el overlay este subido por encima de un modal de la capa
// superior (paso libre); restoreZ() lo devuelve al fondo.
bool s_raised = false;
char s_textBuf[640];

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_shade[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *s_frame = nullptr;
lv_obj_t *s_bubble = nullptr;
lv_obj_t *s_title = nullptr;
lv_obj_t *s_counter = nullptr;
lv_obj_t *s_text = nullptr;
lv_obj_t *s_hint = nullptr;
lv_obj_t *s_feedback = nullptr;
lv_obj_t *s_exitBtn = nullptr, *s_exitLbl = nullptr;
lv_obj_t *s_prevBtn = nullptr, *s_prevLbl = nullptr;
lv_obj_t *s_nextBtn = nullptr, *s_nextLbl = nullptr;
lv_obj_t *s_quizBtn[3] = {nullptr, nullptr, nullptr};
lv_obj_t *s_quizLbl[3] = {nullptr, nullptr, nullptr};
lv_obj_t *s_strip = nullptr;
lv_obj_t *s_stripLbl = nullptr;
lv_obj_t *s_stripExit = nullptr, *s_stripExitLbl = nullptr;

lv_obj_t *makeBtn(lv_obj_t *parent, lv_event_cb_t cb, lv_color_t bg,
                  lv_obj_t **lblOut, void *user = nullptr) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, "");
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user);
  if (lblOut) *lblOut = lbl;
  return btn;
}

void show(lv_obj_t *o, bool on) {
  if (!o) return;
  if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
  else    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

lv_coord_t clampCoord(lv_coord_t v, lv_coord_t lo, lv_coord_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// ---- Gate clinico y cesion -------------------------------------------------

// Sin terapia activa, sin alarma, con enlace y sin apagado. Un paciente
// REGISTRADO (perfil activo en la placa o recordado por la HMI) no impide la
// formacion: la confirmacion de cabina vacia del selector es lo que cubre al
// bebe, y el registro real se avisa alli.
bool gateOk() {
  return !UI_AnyControlActive() && !UI_IsAnyAlarmActive() &&
         !Display_IsBoardLinkLost() && !g_pwrOffActive;
}

// Mismo criterio que TelemetryHistory::mustYield() mas el apagado: una
// leccion no tiene informacion de alarma propia que compense tapar nada.
bool mustYield() {
  return UI_IsAnyAlarmActive() || Display_IsBoardLinkLost() || g_pwrOffActive;
}

const Step &curStep() { return s_lesson->steps[s_idx]; }

// STEP_DO se muestra como EXPLAIN siempre que NO haya sandbox: en demostracion
// y tambien si una leccion sin LESSON_INTERACTIVE trae pasos "hacer" por error.
// Asi la unica forma de que el alumno toque un control real es con la placa
// aislada, por construccion y no por un flag en una tabla.
StepKind effectiveKind(const Step &st) {
  if (st.kind == STEP_DO && !Training_IsActive()) return STEP_EXPLAIN;
  return st.kind;
}

// ---- Sombras / recuadro ------------------------------------------------------

void setShade(int i, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
  if (w <= 0 || h <= 0) {
    show(s_shade[i], false);
    return;
  }
  show(s_shade[i], true);
  lv_obj_set_pos(s_shade[i], x, y);
  lv_obj_set_size(s_shade[i], w, h);
}

// Oscurece toda la pantalla menos [x1,y1,x2,y2] (inclusive), que queda como
// hueco: sin sombra encima y, si la raiz no es clicable, permeable al toque.
void shadeAround(lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2) {
  setShade(0, 0, 0, DISPLAY_WIDTH, y1);
  setShade(1, 0, y2 + 1, DISPLAY_WIDTH, STRIP_Y - y2 - 1);  // hasta la franja
  setShade(2, 0, y1, x1, y2 - y1 + 1);
  setShade(3, x2 + 1, y1, DISPLAY_WIDTH - x2 - 1, y2 - y1 + 1);
}

void shadeAll() {
  setShade(0, 0, 0, DISPLAY_WIDTH, STRIP_Y);
  for (int i = 1; i < 4; i++) show(s_shade[i], false);
}

void hideShades() {
  for (int i = 0; i < 4; i++) show(s_shade[i], false);
}

void placeBubble(lv_coord_t w, lv_coord_t h, const lv_area_t *target) {
  lv_obj_set_size(s_bubble, w, h);
  lv_obj_set_width(s_text, w - 2 * MARGIN);
  if (!target) {
    lv_obj_align(s_bubble, LV_ALIGN_CENTER, 0, -STRIP_H / 2);
    return;
  }
  const lv_coord_t th = lv_area_get_height(target);
  const lv_coord_t cx = target->x1 + lv_area_get_width(target) / 2;
  const lv_coord_t cy = target->y1 + th / 2;
  if (th > TALL_TARGET) {
    lv_obj_set_size(s_bubble, BUBBLE_SIDE_W, BUBBLE_SIDE_H);
    lv_obj_set_width(s_text, BUBBLE_SIDE_W - 2 * MARGIN);
    const lv_coord_t bx = (cx < DISPLAY_WIDTH / 2)
                              ? DISPLAY_WIDTH - BUBBLE_SIDE_W - MARGIN
                              : MARGIN;
    lv_obj_align(s_bubble, LV_ALIGN_TOP_LEFT, bx,
                 (STRIP_Y - BUBBLE_SIDE_H) / 2);
  } else {
    const lv_coord_t by = (cy < DISPLAY_HEIGHT / 2) ? STRIP_Y - h - MARGIN
                                                    : MARGIN;
    lv_obj_align(s_bubble, LV_ALIGN_TOP_LEFT, (DISPLAY_WIDTH - w) / 2, by);
  }
}

// Recuadro y sombras para `target`. `permeable`: el hueco es el area de clic
// del control (incluida su zona tactil ampliada) y la raiz deja pasar el
// toque; si no, el hueco lleva FRAME_PAD y la raiz se lo traga todo.
void frameTarget(lv_obj_t *target, bool permeable, lv_area_t *outArea) {
  lv_area_t a;
  if (permeable) {
    lv_obj_get_click_area(target, &a);
  } else {
    lv_obj_get_coords(target, &a);
  }
  const lv_coord_t hx1 = clampCoord(a.x1 - (permeable ? 0 : FRAME_PAD), 0, DISPLAY_WIDTH - 1);
  const lv_coord_t hy1 = clampCoord(a.y1 - (permeable ? 0 : FRAME_PAD), 0, STRIP_Y - 1);
  const lv_coord_t hx2 = clampCoord(a.x2 + (permeable ? 0 : FRAME_PAD), 0, DISPLAY_WIDTH - 1);
  const lv_coord_t hy2 = clampCoord(a.y2 + (permeable ? 0 : FRAME_PAD), 0, STRIP_Y - 1);
  shadeAround(hx1, hy1, hx2, hy2);

  lv_area_t f;
  lv_obj_get_coords(target, &f);
  const lv_coord_t fx1 = clampCoord(f.x1 - FRAME_PAD, 0, DISPLAY_WIDTH - 1);
  const lv_coord_t fy1 = clampCoord(f.y1 - FRAME_PAD, 0, STRIP_Y - 1);
  const lv_coord_t fx2 = clampCoord(f.x2 + FRAME_PAD, 0, DISPLAY_WIDTH - 1);
  const lv_coord_t fy2 = clampCoord(f.y2 + FRAME_PAD, 0, STRIP_Y - 1);
  show(s_frame, true);
  lv_obj_set_pos(s_frame, fx1, fy1);
  lv_obj_set_size(s_frame, fx2 - fx1 + 1, fy2 - fy1 + 1);
  if (outArea) *outArea = f;
}

void setRootClickable(bool on) {
  if (on) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
  else    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
}

void setStripText(const char *txt) {
  if (s_stripLbl) lv_label_set_text(s_stripLbl, txt);
}

const char *modeStripText() {
  switch (s_mode) {
    case MODE_INTERACTIVE:
      // Lo peligroso delante y corto, para que quepa sin scroll.
      return TR(STR_TRAIN_BANNER_REAL);
    case MODE_DEMO:
      return TR(STR_TRAIN_BANNER_DEMO);
    default:
      return TR(STR_TRAIN_BANNER_PLAIN);
  }
}

// ---- Ciclo de vida -----------------------------------------------------------

void closeAllDialogs() {
  BabyWizard_Cancel();
  BabyExitDialog_Cancel();
  TimeDialog_Close();
  BabyHistory_Close();
  TelemetryHistory_Close();
  if (AlarmCenter_IsOpen()) AlarmCenter_Close();
}

// Devuelve el overlay al fondo de lv_layer_top(): por debajo del banner de
// alarma, del icono AUDIO PAUSED y de los modales. Por identidad, no por
// indice guardado (la capa se reordena en caliente con los toasts).
void restoreZ() {
  if (s_raised && s_overlay) {
    lv_obj_move_background(s_overlay);
    s_raised = false;
  }
}

// `aborted`: cierre por alarma, enlace, apagado o inactividad. Entonces NO se
// reabre el selector ni se muestra nada: la pantalla clinica tiene que quedar
// limpia en el instante en que hace falta.
void endLesson(bool passed, bool aborted) {
  if (!s_open) return;
  const Course *course = s_course;
  const uint8_t lessonIdx = s_lessonIdx;
  const uint16_t attempts = s_attempts;
  const Mode mode = s_mode;

  // Incondicional: el permiso del dialogo de salida no puede sobrevivir a
  // ninguna leccion, tampoco a una en demostracion (donde Training_Exit no
  // se llama).
  Training_SetExitDialogAllowed(false);
  closeAllDialogs();
  if (mode == MODE_INTERACTIVE) {
    // Orden: UI (switches en silencio + UI_SyncAll) -> Training_Exit, que
    // restaura hmi_msg desde la instantanea, fuerza su envio (la placa apaga
    // lo que la leccion encendio) y baja el flag.
    UI_RestoreControlSnapshot(&s_snap);
    Training_Exit();
    // El seq de formacion (0xFFFF) no debe sobrevivir a la leccion. Solo se
    // borra si es el de ZOE: si la leccion no llego a seleccionarla, el
    // perfil recordado (un paciente registrado) se queda como estaba.
    if (BabyWizard_GetActiveSeq() == TRAINING_BABY_SEQ) {
      BabyWizard_ClearActiveProfile();
    }
  }
  if (mode == MODE_DEMO) passed = false;

  restoreZ();
  show(s_overlay, false);
  s_open = false;
  s_mode = MODE_NONE;
  s_course = nullptr;
  s_lesson = nullptr;
  // Si la leccion dejo cargada la pantalla de bloqueo de verdad (candado
  // real en E8/E9), se respeta: ahi si se pinta el banner de alarma. Desde
  // cualquier otra pantalla (Ajustes) se vuelve a la principal, que es donde
  // estan el icono de alarmas y los controles. Se mira la pantalla activa, no
  // la global `locked`, que vale true en la principal casi siempre (la fija
  // unlock_timeout_cb 5 s despues de desbloquear).
  const bool onLock = (ui_ScreenLock && lv_scr_act() == ui_ScreenLock);
  if (ui_ScreenMain && lv_scr_act() != ui_ScreenMain && !onLock) {
    lv_scr_load(ui_ScreenMain);
  }
  lv_disp_trig_activity(NULL);

  if (aborted) {
    TrainingSelector_Close();
    return;
  }
  TrainingSelector_OnLessonEnd(course, lessonIdx, passed, attempts);
}

void enterStep(int idx, int dir);

void onExit(lv_event_t *) { endLesson(false, false); }
void onPrev(lv_event_t *) { enterStep(s_idx - 1, -1); }
void onNext(lv_event_t *) { enterStep(s_idx + 1, +1); }

void onQuizOption(lv_event_t *e) {
  const int pick = (int)(intptr_t)lv_event_get_user_data(e);
  if (!s_open || s_quizSolved) return;
  const Step &st = curStep();
  if (!st.quiz) return;
  if (pick == st.quiz->correct) {
    s_quizSolved = true;
    lv_label_set_text(s_feedback, TR(STR_TRAIN_CORRECT));
    lv_obj_set_style_text_color(s_feedback, lv_color_hex(0x00AA00), 0);
    for (int i = 0; i < 3; i++) {
      lv_obj_set_style_bg_color(s_quizBtn[i],
                                i == pick ? lv_color_hex(0x00AA00)
                                          : lv_color_hex(0xBBBBBB),
                                LV_PART_MAIN);
    }
    show(s_nextBtn, true);
  } else {
    s_attempts++;
    // Explicar y repetir: la pregunta sigue en pantalla con las tres opciones.
    snprintf(s_textBuf, sizeof(s_textBuf), "%s %s",
             TR(STR_TRAIN_WRONG),
             TrainingTxt(st.quiz->explain));
    lv_label_set_text(s_feedback, s_textBuf);
    lv_obj_set_style_text_color(s_feedback, lv_color_hex(0xAA3333), 0);
    lv_obj_set_style_bg_color(s_quizBtn[pick], lv_color_hex(0xAA3333),
                              LV_PART_MAIN);
  }
}

bool prevAllowed(int idx) {
  // Solo se retrocede entre pasos de explicar: un paso "hacer" completado no
  // se deshace (el estado ya cambio) y una pregunta acertada tampoco.
  if (idx <= 0) return false;
  return effectiveKind(s_lesson->steps[idx - 1]) == STEP_EXPLAIN;
}

void render(const Step &st, StepKind kind, lv_obj_t *target) {
  const bool last = (s_idx == s_lesson->stepCount - 1);
  char cnt[16];
  snprintf(cnt, sizeof(cnt), "%d/%d", s_idx + 1, s_lesson->stepCount);
  lv_label_set_text(s_counter, cnt);
  lv_label_set_text(s_title, TrainingTxt(s_lesson->title));
  lv_label_set_text(s_exitLbl, TR(STR_TOUR_EXIT));
  lv_label_set_text(s_prevLbl, TR(STR_TOUR_PREV));
  lv_label_set_text(s_nextLbl, last ? TR(STR_TOUR_FINISH)
                                    : TR(STR_TOUR_NEXT));
  lv_label_set_text(s_stripExitLbl, TR(STR_TOUR_EXIT));
  show(s_feedback, false);
  show(s_hint, false);
  for (int i = 0; i < 3; i++) show(s_quizBtn[i], false);
  setStripText(modeStripText());
  show(s_stripExit, false);
  // Franja opaca y clicable salvo en los pasos libres (ver mas abajo).
  lv_obj_set_style_bg_opa(s_strip, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_add_flag(s_strip, LV_OBJ_FLAG_CLICKABLE);

  // Texto del paso (con prefijo en demostracion cuando el original era "hacer").
  if (st.kind == STEP_DO && s_mode == MODE_DEMO) {
    snprintf(s_textBuf, sizeof(s_textBuf), "%s %s",
             TR(STR_TRAIN_DEMO_PREFIX),
             TrainingTxt(st.text));
    lv_label_set_text(s_text, s_textBuf);
  } else {
    lv_label_set_text(s_text, TrainingTxt(st.text));
  }

  lv_area_t targetArea;
  const lv_area_t *bubbleRef = nullptr;

  if (kind == STEP_DO && (st.flags & STEP_FREE)) {
    // Paso libre: sin sombras ni recuadro, instruccion en la franja. La
    // franja se hace translucida y NO clicable (solo su boton SALIR lo es):
    // el teclado del asistente del bebe llega hasta y=466 y no debe quedar
    // tapado ni bloqueado por ella.
    hideShades();
    show(s_frame, false);
    show(s_bubble, false);
    setRootClickable(false);
    setStripText(TrainingTxt(st.text));
    show(s_stripExit, true);
    lv_obj_set_style_bg_opa(s_strip, LV_OPA_80, LV_PART_MAIN);
    lv_obj_clear_flag(s_strip, LV_OBJ_FLAG_CLICKABLE);
    return;
  }

  show(s_bubble, true);
  if (kind == STEP_QUIZ) {
    shadeAll();
    show(s_frame, false);
    setRootClickable(true);
    placeBubble(QUIZ_W, QUIZ_H, nullptr);
    lv_obj_set_width(s_text, QUIZ_W - 2 * MARGIN);
    for (int i = 0; i < 3; i++) {
      show(s_quizBtn[i], true);
      lv_obj_set_style_bg_color(s_quizBtn[i], lv_color_hex(0x0075EE),
                                LV_PART_MAIN);
      lv_label_set_text(s_quizLbl[i], TrainingTxt(st.quiz->options[i]));
    }
    show(s_feedback, true);
    lv_label_set_text(s_feedback, "");
    show(s_prevBtn, false);
    show(s_nextBtn, s_quizSolved);
    return;
  }

  if (target) {
    frameTarget(target, kind == STEP_DO, &targetArea);
    bubbleRef = &targetArea;
  } else {
    shadeAll();
    show(s_frame, false);
  }
  placeBubble(BUBBLE_W, BUBBLE_H, bubbleRef);

  if (kind == STEP_DO) {
    // Solo el control resaltado recibe toques; el bocadillo solo tiene SALIR.
    setRootClickable(false);
    show(s_prevBtn, false);
    show(s_nextBtn, false);
    show(s_hint, true);
    lv_label_set_text(s_hint,
                      TR(STR_TRAIN_YOUR_TURN));
  } else {
    setRootClickable(true);
    show(s_prevBtn, prevAllowed(s_idx));
    show(s_nextBtn, true);
  }
}

void enterStep(int idx, int dir) {
  if (!s_open || !s_lesson) return;
  if (dir == 0) dir = +1;
  lv_obj_t *target = nullptr;
  StepKind kind = STEP_EXPLAIN;

  // Cota de iteraciones: el bucle salta pasos ocultos o ya cumplidos, y nunca
  // debe poder girar indefinidamente bajo LVGL_Lock() (colgaria la UI).
  int budget = 2 * (int)s_lesson->stepCount + 2;
  while (true) {
    if (--budget < 0) {
      endLesson(false, false);
      return;
    }
    if (idx < 0) {
      // Retrocediendo, el paso 0 no se puede mostrar (control oculto): se
      // sigue hacia delante desde el principio en vez de insistir.
      idx = 0;
      dir = +1;
    }
    if (idx >= s_lesson->stepCount) {
      endLesson(true, false);
      return;
    }
    const Step &st = s_lesson->steps[idx];
    lv_obj_t *scr = st.screen ? *st.screen : nullptr;
    if (scr && lv_scr_act() != scr) lv_scr_load(scr);
    if (scr) lv_obj_update_layout(scr);
    kind = effectiveKind(st);
    target = nullptr;
    if (st.target && !(st.flags & STEP_FREE)) {
      target = *st.target;
      // Control oculto (p. ej. humedad deshabilitada): el paso se salta.
      if (!target || !lv_obj_is_visible(target)) {
        idx += dir;
        continue;
      }
      // Control enteramente bajo la franja inferior: no se podria tocar.
      lv_area_t ca;
      lv_obj_get_click_area(target, &ca);
      if (kind == STEP_DO && ca.y1 >= STRIP_Y) {
        idx += dir;
        continue;
      }
    }
    // Objetivo ya cumplido al entrar: no se pide algo que ya esta hecho.
    if (kind == STEP_DO && st.goal && dir > 0 && st.goal()) {
      idx += dir;
      continue;
    }
    break;
  }

  s_idx = idx;
  s_quizSolved = false;
  const Step &st = curStep();
  // onEnter solo si el paso se va a HACER: en demostracion (degradado a
  // explicar) no debe armar nada (p. ej. el permiso del dialogo de salida).
  if (st.onEnter && kind == st.kind) st.onEnter();
  render(st, kind, target);
}

}  // namespace

// ---- API ---------------------------------------------------------------------

void Training_Init(void) {
  s_overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(s_overlay);
  lv_obj_set_size(s_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_pos(s_overlay, 0, 0);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
  // Clicable por defecto (se traga todo); en los pasos "hacer" se quita para
  // que el hueco del recuadro deje pasar el toque al control real.
  lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < 4; i++) {
    s_shade[i] = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_shade[i]);
    lv_obj_set_style_bg_color(s_shade[i], lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_shade[i], LV_OPA_50, LV_PART_MAIN);
    // Clicables a proposito: se tragan los toques fuera del hueco.
    lv_obj_add_flag(s_shade[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_shade[i], LV_OBJ_FLAG_SCROLLABLE);
  }

  s_frame = lv_obj_create(s_overlay);
  lv_obj_remove_style_all(s_frame);
  lv_obj_set_style_bg_opa(s_frame, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_color(s_frame, lv_color_hex(0xFFC107), LV_PART_MAIN);
  lv_obj_set_style_border_width(s_frame, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(s_frame, 10, LV_PART_MAIN);
  lv_obj_set_style_shadow_color(s_frame, lv_color_hex(0xFFC107), LV_PART_MAIN);
  lv_obj_set_style_shadow_width(s_frame, 24, LV_PART_MAIN);
  lv_obj_set_style_shadow_spread(s_frame, 2, LV_PART_MAIN);
  lv_obj_set_style_shadow_opa(s_frame, LV_OPA_70, LV_PART_MAIN);
  // No clicable: en los pasos "hacer" el toque tiene que atravesarlo.
  lv_obj_clear_flag(s_frame, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(s_frame, LV_OBJ_FLAG_SCROLLABLE);

  s_bubble = lv_obj_create(s_overlay);
  lv_obj_set_size(s_bubble, BUBBLE_W, BUBBLE_H);
  lv_obj_set_style_radius(s_bubble, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_bubble, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_border_color(s_bubble, lv_color_hex(0xFFC107), LV_PART_MAIN);
  lv_obj_set_style_border_width(s_bubble, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_bubble, MARGIN, LV_PART_MAIN);
  lv_obj_clear_flag(s_bubble, LV_OBJ_FLAG_SCROLLABLE);

  s_title = lv_label_create(s_bubble);
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_title, lv_color_hex(0x888888), 0);
  lv_obj_align(s_title, LV_ALIGN_TOP_LEFT, 0, 0);

  s_counter = lv_label_create(s_bubble);
  lv_obj_set_style_text_font(s_counter, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_counter, lv_color_hex(0x888888), 0);
  lv_obj_align(s_counter, LV_ALIGN_TOP_RIGHT, 0, 0);

  s_text = lv_label_create(s_bubble);
  lv_label_set_long_mode(s_text, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_text, BUBBLE_W - 2 * MARGIN);
  lv_obj_set_style_text_font(s_text, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(s_text, lv_color_hex(0x0B2E4F), 0);
  lv_obj_align(s_text, LV_ALIGN_TOP_LEFT, 0, 22);

  // Pista de los pasos "hacer", encima de la fila de botones.
  s_hint = lv_label_create(s_bubble);
  lv_obj_set_style_text_font(s_hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_hint, lv_color_hex(0xE08800), 0);
  lv_obj_align(s_hint, LV_ALIGN_BOTTOM_RIGHT, 0, -(BTN_H + 8));

  // Pregunta: tres opciones apiladas y una linea de respuesta.
  for (int i = 0; i < 3; i++) {
    s_quizBtn[i] = makeBtn(s_bubble, onQuizOption, lv_color_hex(0x0075EE),
                           &s_quizLbl[i], (void *)(intptr_t)i);
    lv_obj_set_size(s_quizBtn[i], QUIZ_W - 2 * MARGIN, BTN_H);
    lv_obj_align(s_quizBtn[i], LV_ALIGN_TOP_LEFT, 0, 96 + i * (BTN_H + 8));
    lv_label_set_long_mode(s_quizLbl[i], LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_quizLbl[i], QUIZ_W - 2 * MARGIN - 20);
    lv_obj_set_style_text_align(s_quizLbl[i], LV_TEXT_ALIGN_CENTER, 0);
  }
  s_feedback = lv_label_create(s_bubble);
  lv_label_set_long_mode(s_feedback, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_feedback, QUIZ_W - 2 * MARGIN);
  lv_obj_set_style_text_font(s_feedback, &lv_font_montserrat_14, 0);
  lv_obj_align(s_feedback, LV_ALIGN_TOP_LEFT, 0, 96 + 3 * (BTN_H + 8) + 4);

  s_exitBtn = makeBtn(s_bubble, onExit, lv_color_hex(0x888888), &s_exitLbl);
  lv_obj_set_size(s_exitBtn, 130, BTN_H);
  lv_obj_align(s_exitBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  s_prevBtn = makeBtn(s_bubble, onPrev, lv_color_hex(0x0075EE), &s_prevLbl);
  lv_obj_set_size(s_prevBtn, 150, BTN_H);
  lv_obj_align(s_prevBtn, LV_ALIGN_BOTTOM_MID, 0, 0);
  s_nextBtn = makeBtn(s_bubble, onNext, lv_color_hex(0x00AA00), &s_nextLbl);
  lv_obj_set_size(s_nextBtn, 150, BTN_H);
  lv_obj_align(s_nextBtn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  // Franja inferior: hija del overlay para que suba y baje con el.
  s_strip = lv_obj_create(s_overlay);
  lv_obj_remove_style_all(s_strip);
  lv_obj_set_size(s_strip, DISPLAY_WIDTH, STRIP_H);
  lv_obj_set_pos(s_strip, 0, STRIP_Y);
  lv_obj_set_style_bg_color(s_strip, lv_color_hex(0xFFC107), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_strip, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_add_flag(s_strip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(s_strip, LV_OBJ_FLAG_SCROLLABLE);
  s_stripLbl = lv_label_create(s_strip);
  lv_label_set_long_mode(s_stripLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(s_stripLbl, DISPLAY_WIDTH - 110);
  lv_obj_set_style_text_font(s_stripLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_stripLbl, lv_color_hex(0x0B2E4F), 0);
  lv_obj_align(s_stripLbl, LV_ALIGN_LEFT_MID, 10, 0);
  s_stripExit = makeBtn(s_strip, onExit, lv_color_hex(0x0B2E4F), &s_stripExitLbl);
  lv_obj_set_size(s_stripExit, 84, STRIP_H - 6);
  lv_obj_align(s_stripExit, LV_ALIGN_RIGHT_MID, -6, 0);
  lv_obj_set_style_text_font(s_stripExitLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_stripExitLbl, lv_color_hex(0xFFFFFF), 0);
}

void Training_StartLesson(const Course *course, uint8_t lessonIdx) {
  if (!s_overlay || s_open || !course || lessonIdx >= course->lessonCount) return;
  s_course = course;
  s_lesson = &course->lessons[lessonIdx];
  s_lessonIdx = lessonIdx;
  s_attempts = 0;
  s_raised = false;
  Training_SetExitDialogAllowed(false);

  if (s_lesson->flags & LESSON_INTERACTIVE) {
    if (gateOk()) {
      UI_GetControlSnapshot(&s_snap);
      Training_Enter();
      s_mode = MODE_INTERACTIVE;
    } else {
      s_mode = MODE_DEMO;
      UI_ShowToast(TR(STR_TRAIN_DEMO_TOAST),
                   4000);
    }
  } else {
    s_mode = MODE_PASSIVE;
  }

  // Los pasos de Ajustes necesitan la pantalla creada (hoy ui_init() la crea;
  // si algun dia pasa a diferida, el motor no perderia esos pasos).
  if (!ui_ScreenSettings) ui_ScreenSettings_screen_init();

  s_open = true;
  show(s_overlay, true);
  enterStep(0, +1);
}

void Training_AbortLesson(void) { endLesson(false, true); }

bool Training_LessonIsOpen(void) { return s_open; }

bool Training_IsOpen(void) { return s_open || TrainingSelector_IsOpen(); }

void Training_Poll(void) {
  // El selector tiene su propia cesion e inactividad (no depende de que haya
  // una leccion abierta).
  TrainingSelector_Poll();
  if (!s_open) return;

  if (mustYield() ||
      lv_disp_get_inactive_time(NULL) > HELP_IDLE_TIMEOUT_MS) {
    endLesson(false, true);
    return;
  }

  const Step &st = curStep();
  const StepKind kind = effectiveKind(st);

  // En un paso libre, si se abre un modal de lv_layer_top() (centro de
  // alarmas, tendencia) el overlay sube por encima para que la franja con la
  // instruccion siga visible, y a continuacion el banner de alarma y el icono
  // AUDIO PAUSED se reafirman por encima de todo; al cerrarse el modal el
  // overlay vuelve al fondo de la capa.
  if (kind == STEP_DO && (st.flags & STEP_FREE)) {
    // El SALIR de la franja se esconde mientras haya un dialogo de la
    // pantalla principal abierto: el teclado del asistente llega hasta y=456
    // y su barra de espacio quedaba debajo del boton (abortaria la leccion
    // con un toque bajo). Siguen valiendo el aborto por alarma e inactividad
    // y la X del propio dialogo.
    const bool mainDialog = BabyWizard_IsOpen() || BabyExitDialog_IsOpen() ||
                            TimeDialog_IsOpen() || HelpDialog_IsOpen();
    show(s_stripExit, !mainDialog);

    const bool modalTop = AlarmCenter_IsOpen() || TelemetryHistory_IsOpen();
    if (modalTop && !s_raised) {
      lv_obj_move_foreground(s_overlay);
      UI_RaiseAlarmIndicators();
      s_raised = true;
    } else if (!modalTop && s_raised) {
      restoreZ();
    }
  } else if (s_raised) {
    restoreZ();
  }

  if (kind == STEP_DO && st.goal && st.goal()) {
    enterStep(s_idx + 1, +1);
  }
}

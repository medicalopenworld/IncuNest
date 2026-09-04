#include "ui/HelpTour.h"

#include <cstdio>

#include "CommTask.h"
#include "UITask.h"
#include "main.h"
#include "ui.h"
#include "ui/HelpDialog.h"  // HELP_IDLE_TIMEOUT_MS

// --- Shared state owned by UITask.cpp (same pattern TimeDialog.cpp uses) ---
namespace {

// Un paso del recorrido. `target` y `screen` son punteros a los globales
// ui_* (no a los objetos) porque esta tabla se construye en tiempo de
// compilacion y los objetos LVGL se crean despues, en ui_init(). Un paso sin
// target (nullptr) es solo texto (bienvenida, despedida).
struct TourStep {
  lv_obj_t **target;
  lv_obj_t **screen;
  ui_str_id_t text;
};

// El texto de cada paso vive en el catalogo (`ui/i18n_strings.def`, seccion
// "tutorial guiado"), no aqui: esta tabla solo dice a que control apunta cada
// paso y en que pantalla esta. Al traducir un paso hay que comprobar que cabe
// en el bocadillo de su modo (~200 caracteres en el ancho, ~180 en el
// estrecho de los contenedores).
const TourStep STEPS[] = {
    {nullptr, &ui_ScreenMain, STR_TOUR_WELCOME},
    {&ui_HelpButton, &ui_ScreenMain, STR_TOUR_HELP_BTN},
    {&ui_ClockButton, &ui_ScreenMain, STR_TOUR_CLOCK},
    {&ui_ConnCont, &ui_ScreenMain, STR_TOUR_CONN},
    {&ui_ImgButton1, &ui_ScreenMain, STR_TOUR_LOCK},
    {&ui_BabiesButton, &ui_ScreenMain, STR_TOUR_BABIES},
    {&ui_AlarmButton, &ui_ScreenMain, STR_TOUR_ALARMS},
    {&ui_CheckImgMain, &ui_ScreenMain, STR_TOUR_CHECK},
    {&ui_TempCont, &ui_ScreenMain, STR_TOUR_TEMP},
    {&ui_TempToggleBtn, &ui_ScreenMain, STR_TOUR_TEMP_TOGGLE},
    {&ui_HumCont, &ui_ScreenMain, STR_TOUR_HUM},
    {&ui_PhotoCont, &ui_ScreenMain, STR_TOUR_PHOTO},
    {&ui_Settings, &ui_ScreenMain, STR_TOUR_SETTINGS},
    {&ui_InfoCont, &ui_ScreenSettings, STR_TOUR_INFO},
    {&ui_WifiCont, &ui_ScreenSettings, STR_TOUR_WIFI},
    {&ui_LanguagesCont, &ui_ScreenSettings, STR_TOUR_LANG},
    {&ui_ModesCont, &ui_ScreenSettings, STR_TOUR_MODES},
    {&ui_ImgButton2, &ui_ScreenSettings, STR_TOUR_BACK_ARROW},
    {nullptr, &ui_ScreenMain, STR_TOUR_END},
};
constexpr int STEP_COUNT = sizeof(STEPS) / sizeof(STEPS[0]);

// Geometria del bocadillo: ancho para controles bajos (heading, filas de
// Ajustes), estrecho y alto para los contenedores que ocupan casi toda la
// altura de la pantalla, donde se coloca al lado y no encima/debajo.
constexpr lv_coord_t BUBBLE_W = 560, BUBBLE_H = 200;
constexpr lv_coord_t BUBBLE_SIDE_W = 400, BUBBLE_SIDE_H = 280;
constexpr lv_coord_t FRAME_PAD = 6;
constexpr lv_coord_t MARGIN = 16;
constexpr lv_coord_t BTN_H = 44;
// Un control mas alto que esto no deja sitio ni arriba ni abajo.
constexpr lv_coord_t TALL_TARGET = 240;

bool s_open = false;
int s_idx = 0;

lv_obj_t *s_overlay = nullptr;
// Atenuado en cuatro sombras (arriba, abajo, izquierda, derecha) alrededor del
// recuadro, en vez de una capa unica: asi el control resaltado queda SIN
// atenuar, con su brillo normal, y todo lo demas oscurecido.
lv_obj_t *s_shade[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *s_frame = nullptr;
lv_obj_t *s_bubble = nullptr;
lv_obj_t *s_text = nullptr;
lv_obj_t *s_counter = nullptr;
lv_obj_t *s_prevBtn = nullptr;
lv_obj_t *s_nextBtn = nullptr;
lv_obj_t *s_nextLbl = nullptr;
lv_obj_t *s_exitLbl = nullptr;
lv_obj_t *s_prevLbl = nullptr;


lv_obj_t *makeBtn(lv_obj_t *parent, lv_event_cb_t cb, lv_color_t bg,
                  lv_obj_t **lblOut) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, "");
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  if (lblOut) *lblOut = lbl;
  return btn;
}

void stopTour() {
  if (!s_open) return;
  s_open = false;
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  // El recorrido pudo dejar cargada la pantalla de Ajustes; el operador sale
  // siempre a la principal, que es donde estaba al entrar.
  if (ui_ScreenMain && lv_scr_act() != ui_ScreenMain) lv_scr_load(ui_ScreenMain);
  // El auto-bloqueo estuvo en pausa: que vuelva a contar desde cero.
  lv_disp_trig_activity(NULL);
}

void showStep(int idx, int dir);

void onExit(lv_event_t *) { stopTour(); }
void onPrev(lv_event_t *) { showStep(s_idx - 1, -1); }
void onNext(lv_event_t *) { showStep(s_idx + 1, +1); }

lv_coord_t clampCoord(lv_coord_t v, lv_coord_t lo, lv_coord_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void setShade(int i, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
  if (w <= 0 || h <= 0) {
    lv_obj_add_flag(s_shade[i], LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_clear_flag(s_shade[i], LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(s_shade[i], x, y);
  lv_obj_set_size(s_shade[i], w, h);
}

// Oscurece toda la pantalla menos el rectangulo [fx,fy,fx2,fy2] (inclusive).
void shadeAround(lv_coord_t fx, lv_coord_t fy, lv_coord_t fx2, lv_coord_t fy2) {
  setShade(0, 0, 0, DISPLAY_WIDTH, fy);                              // arriba
  setShade(1, 0, fy2 + 1, DISPLAY_WIDTH, DISPLAY_HEIGHT - fy2 - 1);  // abajo
  setShade(2, 0, fy, fx, fy2 - fy + 1);                              // izq.
  setShade(3, fx2 + 1, fy, DISPLAY_WIDTH - fx2 - 1, fy2 - fy + 1);   // der.
}

void shadeAll() {
  setShade(0, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  for (int i = 1; i < 4; i++) lv_obj_add_flag(s_shade[i], LV_OBJ_FLAG_HIDDEN);
}

// Coloca sombras, marco y bocadillo para el control `target` (o solo el
// bocadillo, centrado sobre la pantalla atenuada, si no hay control).
void layoutFor(lv_obj_t *target) {
  if (!target) {
    shadeAll();
    lv_obj_add_flag(s_frame, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(s_bubble, BUBBLE_W, BUBBLE_H);
    lv_obj_set_width(s_text, BUBBLE_W - 2 * MARGIN);
    lv_obj_align(s_bubble, LV_ALIGN_CENTER, 0, 0);
    return;
  }

  lv_area_t a;
  lv_obj_get_coords(target, &a);
  const lv_coord_t tw = lv_area_get_width(&a);
  const lv_coord_t th = lv_area_get_height(&a);

  // Marco: el control mas FRAME_PAD por cada lado, sin salirse de pantalla.
  const lv_coord_t fx = clampCoord(a.x1 - FRAME_PAD, 0, DISPLAY_WIDTH - 1);
  const lv_coord_t fy = clampCoord(a.y1 - FRAME_PAD, 0, DISPLAY_HEIGHT - 1);
  const lv_coord_t fx2 = clampCoord(a.x2 + FRAME_PAD, 0, DISPLAY_WIDTH - 1);
  const lv_coord_t fy2 = clampCoord(a.y2 + FRAME_PAD, 0, DISPLAY_HEIGHT - 1);
  lv_obj_clear_flag(s_frame, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(s_frame, fx, fy);
  lv_obj_set_size(s_frame, fx2 - fx + 1, fy2 - fy + 1);
  shadeAround(fx, fy, fx2, fy2);

  const lv_coord_t cx = a.x1 + tw / 2;
  const lv_coord_t cy = a.y1 + th / 2;

  if (th > TALL_TARGET) {
    // Control alto: bocadillo al lado contrario, pegado al borde.
    lv_obj_set_size(s_bubble, BUBBLE_SIDE_W, BUBBLE_SIDE_H);
    lv_obj_set_width(s_text, BUBBLE_SIDE_W - 2 * MARGIN);
    const lv_coord_t bx = (cx < DISPLAY_WIDTH / 2)
                              ? DISPLAY_WIDTH - BUBBLE_SIDE_W - MARGIN
                              : MARGIN;
    lv_obj_align(s_bubble, LV_ALIGN_TOP_LEFT, bx,
                 (DISPLAY_HEIGHT - BUBBLE_SIDE_H) / 2);
  } else {
    // Control bajo: bocadillo en la mitad de pantalla opuesta.
    lv_obj_set_size(s_bubble, BUBBLE_W, BUBBLE_H);
    lv_obj_set_width(s_text, BUBBLE_W - 2 * MARGIN);
    const lv_coord_t by = (cy < DISPLAY_HEIGHT / 2)
                              ? DISPLAY_HEIGHT - BUBBLE_H - MARGIN
                              : MARGIN;
    lv_obj_align(s_bubble, LV_ALIGN_TOP_LEFT, (DISPLAY_WIDTH - BUBBLE_W) / 2,
                 by);
  }
}

void showStep(int idx, int dir) {
  if (!s_overlay) return;
  if (dir == 0) dir = +1;

  // Salta los pasos cuyo control no este visible ahora (p. ej. humedad
  // deshabilitada en Ajustes, o el check de "todo OK" con alarmas activas).
  lv_obj_t *target = nullptr;
  while (idx >= 0 && idx < STEP_COUNT) {
    const TourStep &st = STEPS[idx];
    lv_obj_t *scr = st.screen ? *st.screen : nullptr;
    if (scr && lv_scr_act() != scr) {
      // Sin animacion: los coords del control tienen que ser validos en esta
      // misma pasada, y lv_obj_update_layout() lo garantiza sobre la pantalla
      // ya cargada.
      lv_scr_load(scr);
    }
    if (scr) lv_obj_update_layout(scr);
    if (!st.target) {
      target = nullptr;
      break;
    }
    target = *st.target;
    if (target && lv_obj_is_visible(target)) break;
    idx += dir;
  }
  if (idx < 0) idx = 0;
  if (idx >= STEP_COUNT) {
    stopTour();
    return;
  }
  s_idx = idx;

  const TourStep &st = STEPS[idx];
  lv_label_set_text(s_text, TR(st.text));
  char cnt[16];
  snprintf(cnt, sizeof(cnt), "%d/%d", idx + 1, STEP_COUNT);
  lv_label_set_text(s_counter, cnt);

  lv_label_set_text(s_exitLbl, TR(STR_TOUR_EXIT));
  lv_label_set_text(s_prevLbl, TR(STR_TOUR_PREV));
  const bool last = (idx == STEP_COUNT - 1);
  lv_label_set_text(s_nextLbl,
                    last ? TR(STR_TOUR_FINISH) : TR(STR_TOUR_NEXT));
  if (idx == 0) {
    lv_obj_add_flag(s_prevBtn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(s_prevBtn, LV_OBJ_FLAG_HIDDEN);
  }

  layoutFor(target);
  // Sin lv_obj_move_foreground(): el overlay se crea en HelpTour_Init() ANTES
  // que el banner de alarma y el icono de AUDIO PAUSED (UITask.cpp), asi que
  // por orden de creacion queda debajo de ellos en lv_layer_top(). Subirlo
  // aqui los taparia, y el banner solo vuelve a primer plano cuando cambia
  // su texto.
}

}  // namespace

void HelpTour_Init(void) {
  // En lv_layer_top() y no en una pantalla: el recorrido cambia de pantalla
  // (Main <-> Ajustes) y el overlay tiene que seguir encima de la que este.
  s_overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(s_overlay);
  lv_obj_set_size(s_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_pos(s_overlay, 0, 0);
  // El overlay en si es transparente: el atenuado lo ponen las cuatro sombras
  // de alrededor del recuadro, para que el control resaltado quede con su
  // brillo normal y el resto oscuro.
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
  // Clickable a proposito: se traga todos los toques que no caigan en el
  // bocadillo, asi durante el recorrido no se acciona nada (tambien los que
  // caen dentro del recuadro, que es transparente pero hijo del overlay).
  lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < 4; i++) {
    s_shade[i] = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_shade[i]);
    lv_obj_set_style_bg_color(s_shade[i], lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_shade[i], LV_OPA_50, LV_PART_MAIN);
    lv_obj_clear_flag(s_shade[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_shade[i], LV_OBJ_FLAG_SCROLLABLE);
  }

  // Marco ambar con halo del mismo color: junto con el hueco sin atenuar,
  // hace que el control resaltado "brille" respecto al resto.
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

  lv_obj_t *exitBtn = makeBtn(s_bubble, onExit, lv_color_hex(0x888888),
                              &s_exitLbl);
  lv_obj_set_size(exitBtn, 130, BTN_H);
  lv_obj_align(exitBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  s_prevBtn = makeBtn(s_bubble, onPrev, lv_color_hex(0x0075EE), &s_prevLbl);
  lv_obj_set_size(s_prevBtn, 150, BTN_H);
  lv_obj_align(s_prevBtn, LV_ALIGN_BOTTOM_MID, 0, 0);

  s_nextBtn = makeBtn(s_bubble, onNext, lv_color_hex(0x00AA00), &s_nextLbl);
  lv_obj_set_size(s_nextBtn, 150, BTN_H);
  lv_obj_align(s_nextBtn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

void HelpTour_Start(void) {
  if (!s_overlay) return;
  // Los pasos de Ajustes necesitan la pantalla creada; ui_init() la crea al
  // arrancar, pero si algun dia pasa a ser diferida (como hace
  // _ui_screen_change con su target_init), el tutorial no perderia esos
  // pasos en silencio.
  if (!ui_ScreenSettings) ui_ScreenSettings_screen_init();
  s_open = true;
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  showStep(0, +1);
}

void HelpTour_Stop(void) { stopTour(); }

bool HelpTour_IsOpen(void) { return s_open; }

void HelpTour_Poll(void) {
  if (!s_open) return;
  // Mismo criterio que TelemetryHistory::mustYield() y que HelpDialog: el
  // recorrido no tiene informacion de alarma propia y su overlay se traga los
  // toques, asi que cede ante CUALQUIER alarma activa o enlace perdido (no
  // solo la lista fija de UI_IsCriticalAlarmActive) y devuelve la principal.
  // Y un recorrido olvidado se cierra solo para devolverle el control al
  // auto-bloqueo.
  if (UI_IsAnyAlarmActive() || Display_IsBoardLinkLost() ||
      lv_disp_get_inactive_time(NULL) > HELP_IDLE_TIMEOUT_MS) {
    stopTour();
  }
}

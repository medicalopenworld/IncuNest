#include "ui/HelpTour.h"

#include <cmath>
#include <cstdio>

#include "CommTask.h"
#include "UITask.h"
#include "main.h"
#include "ui.h"
#include "ui/HelpDialog.h"  // HELP_IDLE_TIMEOUT_MS

// --- Shared state owned by UITask.cpp (same pattern TimeDialog.cpp uses) ---
extern ui_lang_t g_lang;

namespace {

// Un paso del recorrido. `target` y `screen` son punteros a los globales
// ui_* (no a los objetos) porque esta tabla se construye en tiempo de
// compilacion y los objetos LVGL se crean despues, en ui_init(). Un paso sin
// target (nullptr) es solo texto (bienvenida, despedida).
struct TourStep {
  lv_obj_t **target;
  lv_obj_t **screen;
  const char *es;
  const char *en;
  const char *fr;
};

// Textos ASCII sin acentos (las fuentes compiladas no tienen esos glifos).
// Cada texto cabe en el bocadillo de su modo (design.md, decision 5): ~200
// caracteres en el bocadillo ancho, ~180 en el estrecho de los contenedores.
const TourStep STEPS[] = {
    {nullptr, &ui_ScreenMain,
     "Bienvenido al tutorial de IncuNest. Te mostraremos cada control de la "
     "pantalla paso a paso. Pulsa SIGUIENTE para avanzar o SALIR en cualquier "
     "momento. Durante el recorrido no se acciona nada.",
     "Welcome to the IncuNest tutorial. We will show you each control on the "
     "screen step by step. Press NEXT to move on or EXIT at any time. Nothing "
     "is operated during the tour.",
     "Bienvenue dans le tutoriel IncuNest. Nous allons vous montrer chaque "
     "commande de l'ecran pas a pas. Appuyez sur SUIVANT pour avancer ou "
     "QUITTER a tout moment. Rien n'est actionne pendant la visite."},
    {&ui_HelpButton, &ui_ScreenMain,
     "Boton de ayuda. Desde aqui abres este tutorial, el codigo QR del video "
     "tutorial y el formulario para contactar con soporte tecnico.",
     "Help button. From here you open this tutorial, the video tutorial QR "
     "code and the form to contact technical support.",
     "Bouton d'aide. D'ici vous ouvrez ce tutoriel, le code QR du tutoriel "
     "video et le formulaire pour contacter le support technique."},
    {&ui_ClockButton, &ui_ScreenMain,
     "Hora y fecha del equipo. Tocalo para ajustar la hora a mano: de ella "
     "dependen las marcas de tiempo del historial clinico.",
     "Device time and date. Tap it to set the time manually: the clinical "
     "history timestamps depend on it.",
     "Heure et date de l'appareil. Touchez pour regler l'heure a la main : "
     "les horodatages de l'historique clinique en dependent."},
    {&ui_ConnCont, &ui_ScreenMain,
     "Indicador de conectividad: WIFI o 2G y las barras de cobertura con el "
     "servidor de monitorizacion. Si se pierde el enlace con la placa de "
     "control, aparece tachado.",
     "Connectivity indicator: WIFI or 2G and the signal bars to the "
     "monitoring server. If the link with the control board is lost, it shows "
     "crossed out.",
     "Indicateur de connectivite : WIFI ou 2G et les barres de couverture "
     "vers le serveur de suivi. Si la liaison avec la carte de controle est "
     "perdue, il apparait barre."},
    {&ui_ImgButton1, &ui_ScreenMain,
     "Candado. Un toque bloquea la pantalla para evitar toques accidentales. "
     "Tras 20 segundos sin tocar se bloquea sola; el anillo muestra el tiempo "
     "que falta.",
     "Padlock. One tap locks the screen to avoid accidental touches. After 20 "
     "seconds without touching it locks by itself; the ring shows the time "
     "left.",
     "Cadenas. Un appui verrouille l'ecran pour eviter les touches "
     "accidentelles. Apres 20 secondes sans contact il se verrouille seul ; "
     "l'anneau montre le temps restant."},
    {&ui_BabiesButton, &ui_ScreenMain,
     "Bebes: alta de un nuevo paciente (nombre, semanas de gestacion, peso), "
     "historial de estancias y curva de peso de cada bebe.",
     "Babies: admit a new patient (name, gestational weeks, weight), stay "
     "history and weight curve of each baby.",
     "Bebes : admission d'un nouveau patient (nom, semaines de gestation, "
     "poids), historique des sejours et courbe de poids de chaque bebe."},
    {&ui_AlarmButton, &ui_ScreenMain,
     "Alarmas: lista de las alarmas activas con la accion recomendada, y "
     "registro de las pasadas. El numero rojo indica cuantas hay activas.",
     "Alarms: list of active alarms with the recommended action, and log of "
     "past ones. The red number shows how many are active.",
     "Alarmes : liste des alarmes actives avec l'action recommandee, et "
     "journal des precedentes. Le chiffre rouge indique combien sont actives."},
    {&ui_CheckImgMain, &ui_ScreenMain,
     "Este check significa que no hay ninguna alarma activa. Tocarlo tambien "
     "abre el registro de alarmas.",
     "This check mark means there is no active alarm. Tapping it also opens "
     "the alarm log.",
     "Cette coche signifie qu'aucune alarme n'est active. La toucher ouvre "
     "aussi le journal des alarmes."},
    {&ui_TempCont, &ui_ScreenMain,
     "Control de temperatura. Elige AIRE (cabina) o PIEL (sonda en el bebe). "
     "Las flechas ajustan la consigna; las cifras grandes son la medida "
     "actual.",
     "Temperature control. Choose AIR (cabin) or SKIN (probe on the baby). "
     "The arrows adjust the setpoint; the big figures are the current "
     "reading.",
     "Controle de temperature. Choisissez AIR (habitacle) ou PEAU (sonde sur "
     "le bebe). Les fleches reglent la consigne ; les grands chiffres sont la "
     "mesure actuelle."},
    {&ui_TempToggleBtn, &ui_ScreenMain,
     "Con este boton activas o desactivas el control de temperatura. Al "
     "activarlo se te pediran los datos del bebe si aun no estan.",
     "This button turns temperature control on or off. When turning it on "
     "you will be asked for the baby data if not yet entered.",
     "Ce bouton active ou desactive le controle de temperature. A "
     "l'activation, les donnees du bebe vous seront demandees si elles "
     "manquent."},
    {&ui_HumCont, &ui_ScreenMain,
     "Control de humedad: la consigna con las flechas y el boton inferior "
     "para activarlo. Se puede ocultar desde Ajustes > Modos.",
     "Humidity control: setpoint with the arrows and the lower button to "
     "turn it on. It can be hidden from Settings > Modes.",
     "Controle d'humidite : la consigne avec les fleches et le bouton du bas "
     "pour l'activer. Il peut etre masque depuis Reglages > Modes."},
    {&ui_PhotoCont, &ui_ScreenMain,
     "Fototerapia: fija los minutos con + y - e INICIAR arranca la cuenta "
     "atras. Cubre siempre los ojos del bebe antes de encender la luz.",
     "Phototherapy: set the minutes with + and -, START begins the "
     "countdown. Always cover the baby's eyes before switching the light on.",
     "Phototherapie : reglez les minutes avec + et -, DEMARRER lance le "
     "compte a rebours. Couvrez toujours les yeux du bebe avant d'allumer."},
    {&ui_Settings, &ui_ScreenMain,
     "Ajustes: informacion del equipo, WiFi, idioma y modos de funcionamiento. "
     "Vamos a verlo.",
     "Settings: device information, WiFi, language and operating modes. "
     "Let's have a look.",
     "Reglages : informations sur l'appareil, WiFi, langue et modes de "
     "fonctionnement. Allons voir."},
    {&ui_InfoCont, &ui_ScreenSettings,
     "Informacion: numero de serie y versiones de firmware de la pantalla y de "
     "la placa. Tenlo a mano al contactar con soporte.",
     "Information: serial number and firmware versions of the display and the "
     "board. Keep it handy when contacting support.",
     "Informations : numero de serie et versions du firmware de l'ecran et de "
     "la carte. Gardez-les a portee de main pour contacter le support."},
    {&ui_WifiCont, &ui_ScreenSettings,
     "WiFi: red y contrasena para conectar el equipo al servidor de "
     "monitorizacion y recibir actualizaciones de firmware.",
     "WiFi: network and password to connect the device to the monitoring "
     "server and receive firmware updates.",
     "WiFi : reseau et mot de passe pour connecter l'appareil au serveur de "
     "suivi et recevoir les mises a jour du firmware."},
    {&ui_LanguagesCont, &ui_ScreenSettings,
     "Idioma: espanol, ingles o frances. Se aplica al instante a toda la "
     "pantalla y a la placa de control.",
     "Language: Spanish, English or French. It applies at once to the whole "
     "screen and to the control board.",
     "Langue : espagnol, anglais ou francais. Elle s'applique aussitot a tout "
     "l'ecran et a la carte de controle."},
    {&ui_ModesCont, &ui_ScreenSettings,
     "Modos: control por piel, modo oscuro y control de humedad. Lo que "
     "desactives aqui desaparece de la pantalla principal.",
     "Modes: skin control, dark mode and humidity control. Whatever you "
     "disable here disappears from the main screen.",
     "Modes : controle par la peau, mode sombre et controle d'humidite. Ce "
     "que vous desactivez ici disparait de l'ecran principal."},
    {&ui_ImgButton2, &ui_ScreenSettings,
     "Con esta flecha vuelves a la pantalla principal.",
     "This arrow takes you back to the main screen.",
     "Cette fleche vous ramene a l'ecran principal."},
    {nullptr, &ui_ScreenMain,
     "Fin del tutorial. Puedes repetirlo cuando quieras desde el boton de "
     "ayuda. Si tienes dudas, usa CONTACTAR SOPORTE en ese mismo menu.",
     "End of the tutorial. You can repeat it any time from the help button. "
     "If in doubt, use CONTACT SUPPORT in that same menu.",
     "Fin du tutoriel. Vous pouvez le refaire a tout moment depuis le bouton "
     "d'aide. En cas de doute, utilisez CONTACTER LE SUPPORT dans ce menu."},
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

// Flecha del bocadillo al recuadro: longitud y semiancho de la punta.
constexpr lv_coord_t ARROW_HEAD_LEN = 18;
constexpr lv_coord_t ARROW_HEAD_HALF = 11;
constexpr lv_coord_t ARROW_GAP = 6;

bool s_open = false;
int s_idx = 0;

lv_obj_t *s_overlay = nullptr;
// Atenuado en cuatro sombras (arriba, abajo, izquierda, derecha) alrededor del
// recuadro, en vez de una capa unica: asi el control resaltado queda SIN
// atenuar, con su brillo normal, y todo lo demas oscurecido.
lv_obj_t *s_shade[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *s_frame = nullptr;
lv_obj_t *s_arrow = nullptr;
lv_point_t s_arrowPts[5];
lv_obj_t *s_bubble = nullptr;
lv_obj_t *s_text = nullptr;
lv_obj_t *s_counter = nullptr;
lv_obj_t *s_prevBtn = nullptr;
lv_obj_t *s_nextBtn = nullptr;
lv_obj_t *s_nextLbl = nullptr;
lv_obj_t *s_exitLbl = nullptr;
lv_obj_t *s_prevLbl = nullptr;

const char *TXT(const char *es, const char *en, const char *fr) {
  return (g_lang == LANG_ES) ? es : (g_lang == LANG_FR) ? fr : en;
}

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

// Flecha desde el borde del bocadillo mas cercano al recuadro hasta el borde
// del recuadro mas cercano al bocadillo, con punta. lv_line dibuja la
// polilinea P0->P1 (astil), P1->P2, P2->P3, P3->P4 (punta) en coordenadas de
// pantalla porque el objeto vive en (0,0) del overlay.
void drawArrow(lv_coord_t fx, lv_coord_t fy, lv_coord_t fx2, lv_coord_t fy2) {
  lv_obj_update_layout(s_bubble);
  lv_area_t b;
  lv_obj_get_coords(s_bubble, &b);

  // Centros de recuadro y bocadillo.
  const lv_coord_t fcx = (fx + fx2) / 2, fcy = (fy + fy2) / 2;
  const lv_coord_t bcx = (b.x1 + b.x2) / 2, bcy = (b.y1 + b.y2) / 2;

  lv_coord_t sx, sy, ex, ey;
  const bool horizontal = (fx > b.x2) || (fx2 < b.x1);
  if (horizontal) {
    // Recuadro a un lado del bocadillo: flecha horizontal.
    sy = bcy;
    ey = clampCoord(bcy, fy + FRAME_PAD, fy2 - FRAME_PAD);
    if (fx > b.x2) {
      sx = b.x2;
      ex = fx - ARROW_GAP;
    } else {
      sx = b.x1;
      ex = fx2 + ARROW_GAP;
    }
    (void)fcy;
  } else {
    // Recuadro encima o debajo: flecha vertical hacia el centro del recuadro.
    sx = clampCoord(fcx, b.x1 + MARGIN, b.x2 - MARGIN);
    ex = fcx;
    if (fy > b.y2) {
      sy = b.y2;
      ey = fy - ARROW_GAP;
    } else {
      sy = b.y1;
      ey = fy2 + ARROW_GAP;
    }
    (void)bcx;
  }

  // Direccion unitaria (astil) y normal, para la punta.
  const float dx = (float)(ex - sx), dy = (float)(ey - sy);
  const float len = sqrtf(dx * dx + dy * dy);
  if (len < (float)(ARROW_HEAD_LEN + ARROW_GAP)) {
    // Demasiado corta para que se lea como flecha: no se pinta.
    lv_obj_add_flag(s_arrow, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  const float ux = dx / len, uy = dy / len;
  const float nx = -uy, ny = ux;

  s_arrowPts[0] = {sx, sy};
  s_arrowPts[1] = {ex, ey};
  s_arrowPts[2] = {(lv_coord_t)(ex - ux * ARROW_HEAD_LEN + nx * ARROW_HEAD_HALF),
                   (lv_coord_t)(ey - uy * ARROW_HEAD_LEN + ny * ARROW_HEAD_HALF)};
  s_arrowPts[3] = {ex, ey};
  s_arrowPts[4] = {(lv_coord_t)(ex - ux * ARROW_HEAD_LEN - nx * ARROW_HEAD_HALF),
                   (lv_coord_t)(ey - uy * ARROW_HEAD_LEN - ny * ARROW_HEAD_HALF)};
  lv_line_set_points(s_arrow, s_arrowPts, 5);
  lv_obj_clear_flag(s_arrow, LV_OBJ_FLAG_HIDDEN);
}

// Coloca sombras, marco, flecha y bocadillo para el control `target` (o solo
// el bocadillo, centrado sobre la pantalla atenuada, si no hay control).
void layoutFor(lv_obj_t *target) {
  if (!target) {
    shadeAll();
    lv_obj_add_flag(s_frame, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_arrow, LV_OBJ_FLAG_HIDDEN);
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

  drawArrow(fx, fy, fx2, fy2);
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
  lv_label_set_text(s_text, TXT(st.es, st.en, st.fr));
  char cnt[16];
  snprintf(cnt, sizeof(cnt), "%d/%d", idx + 1, STEP_COUNT);
  lv_label_set_text(s_counter, cnt);

  lv_label_set_text(s_exitLbl, TXT("SALIR", "EXIT", "QUITTER"));
  lv_label_set_text(s_prevLbl, TXT("ANTERIOR", "BACK", "PRECEDENT"));
  const bool last = (idx == STEP_COUNT - 1);
  lv_label_set_text(s_nextLbl, last ? TXT("TERMINAR", "FINISH", "TERMINER")
                                    : TXT("SIGUIENTE", "NEXT", "SUIVANT"));
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

  // Flecha del bocadillo al recuadro (puntos en coordenadas de pantalla).
  s_arrow = lv_line_create(s_overlay);
  lv_obj_set_pos(s_arrow, 0, 0);
  lv_obj_set_style_line_color(s_arrow, lv_color_hex(0xFFC107), LV_PART_MAIN);
  lv_obj_set_style_line_width(s_arrow, 5, LV_PART_MAIN);
  lv_obj_set_style_line_rounded(s_arrow, true, LV_PART_MAIN);
  lv_obj_clear_flag(s_arrow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(s_arrow, LV_OBJ_FLAG_HIDDEN);

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

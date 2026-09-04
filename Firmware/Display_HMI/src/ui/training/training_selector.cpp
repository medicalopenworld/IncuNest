// Selector de cursos (spec hmi-training-courses, design decision 5): cursos,
// alumno, lecciones con estado, certificados. Modal sobre ui_ScreenMain con
// el patron de TimeDialog/HelpDialog.
#include "ui/training/training.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "CommTask.h"
#include "Credentials_public.h"
#include "UITask.h"
#include "main.h"
#include "modules/support/support_report.h"
#include "ui.h"
#include "ui/HelpDialog.h"  // HELP_IDLE_TIMEOUT_MS
#include "ui/training/lessons.h"
#include "ui/training/training_progress.h"

extern ui_lang_t g_lang;

namespace {

enum View { V_COURSES, V_CONTINUE, V_NAME, V_LESSONS, V_CERTS, V_CERT };

constexpr lv_coord_t CARD_W = 780, CARD_H = 460;
constexpr lv_coord_t QR_SIZE = 300;
constexpr size_t MAILTO_MAX = 900;

bool s_open = false;
View s_view = V_COURSES;
const Course *s_course = nullptr;
uint8_t s_certIdx = 0;
char s_name[TRAINING_NAME_LEN];
char s_mailto[MAILTO_MAX];

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_card = nullptr;
lv_obj_t *s_content = nullptr;
lv_obj_t *s_closeBtn = nullptr;
lv_obj_t *s_nameTa = nullptr;

// Mismo teclado de letras que BabyWizard.cpp (lv_btnmatrix, no lv_keyboard:
// en LVGL 8.3 el keymap de lv_keyboard es un global de fichero). Sin coma:
// el nombre acaba en un asunto de correo y en NVS, y la coma es el
// separador del protocolo serie por si algun dia viaja.
const char *KB_LETTERS_MAP[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    " ", ""};
const lv_btnmatrix_ctrl_t KB_LETTERS_CTRL[28] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1};

const char *TXT(const char *es, const char *en, const char *fr) {
  return (g_lang == LANG_ES) ? es : (g_lang == LANG_FR) ? fr : en;
}

lv_obj_t *makeBtn(lv_obj_t *parent, const char *text, lv_event_cb_t cb,
                  lv_color_t bg, void *user = nullptr) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user);
  return btn;
}

lv_obj_t *makeTitle(const char *text) {
  lv_obj_t *title = lv_label_create(s_content);
  lv_label_set_text(title, text);
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(title, CARD_W - 20 - 60);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);
  return title;
}

lv_obj_t *makeWrapLabel(lv_obj_t *parent, const char *text, lv_coord_t x,
                        lv_coord_t y, lv_coord_t w, const lv_font_t *font,
                        lv_color_t color) {
  lv_obj_t *lbl = lv_label_create(parent);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl, w);
  lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_color(lbl, color, 0);
  lv_label_set_text(lbl, text);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, x, y);
  return lbl;
}

void showView(View v);

void closeDialog() {
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  if (s_content) lv_obj_clean(s_content);
  s_nameTa = nullptr;
  s_open = false;
  lv_disp_trig_activity(NULL);
}

void openAt(View v) {
  if (!s_overlay) return;
  showView(v);
  s_open = true;
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
}

// ---- Callbacks ----------------------------------------------------------------

void onClose(lv_event_t *) { closeDialog(); }
void onBackCourses(lv_event_t *) { showView(V_COURSES); }
void onCerts(lv_event_t *) { showView(V_CERTS); }

void onCourse(lv_event_t *e) {
  const uint8_t idx = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  s_course = Training_CourseByIndex(idx);
  if (!s_course) return;
  const TrainingCourseProgress *p = TrainingProgress_Get(idx);
  showView((p && p->name[0]) ? V_CONTINUE : V_NAME);
}

void onContinue(lv_event_t *) { showView(V_LESSONS); }
void onNewStudent(lv_event_t *) { showView(V_NAME); }

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

void onNameContinue(lv_event_t *) {
  if (!s_nameTa || !s_course) return;
  const char *t = lv_textarea_get_text(s_nameTa);
  // Sin espacios de relleno: dos letras utiles como minimo.
  size_t letters = 0;
  for (const char *p = t; p && *p; p++) if (*p != ' ') letters++;
  if (letters < 2) {
    UI_ShowToast(TXT("Escribe al menos dos letras", "Type at least two letters",
                     "Saisissez au moins deux lettres"),
                 2500);
    return;
  }
  strncpy(s_name, t, TRAINING_NAME_LEN - 1);
  s_name[TRAINING_NAME_LEN - 1] = '\0';
  TrainingProgress_StartCourse(s_course->id, s_name);
  showView(V_LESSONS);
}

void onLesson(lv_event_t *e) {
  const uint8_t idx = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (!s_course || idx >= s_course->lessonCount) return;
  const Course *course = s_course;
  closeDialog();
  Training_StartLesson(course, idx);
}

void onCert(lv_event_t *e) {
  s_certIdx = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  showView(V_CERT);
}

// ---- Vistas -------------------------------------------------------------------

void progressLine(char *out, size_t cap, uint8_t courseIdx) {
  const Course *c = Training_CourseByIndex(courseIdx);
  const TrainingCourseProgress *p = TrainingProgress_Get(courseIdx);
  if (!c || !p || !p->name[0]) {
    snprintf(out, cap, "%s", TXT("Sin alumno en curso", "No student in progress",
                                 "Aucun eleve en cours"));
    return;
  }
  uint8_t done = 0;
  for (uint8_t i = 0; i < c->lessonCount; i++)
    if (TrainingProgress_IsLessonDone(courseIdx, i)) done++;
  snprintf(out, cap, "%s: %u/%u", p->name, (unsigned)done,
           (unsigned)c->lessonCount);
}

void buildCourses() {
  makeTitle(TXT("CURSOS DE FORMACION", "TRAINING COURSES", "COURS DE FORMATION"));

  // Dos tarjetas de 360 con 16 de hueco: 6 + 360 + 16 + 360 = 742 de 760.
  for (uint8_t i = 0; i < TRAINING_COURSES; i++) {
    const Course *c = Training_CourseByIndex(i);
    if (!c) continue;
    lv_obj_t *btn = lv_btn_create(s_content);
    lv_obj_set_size(btn, 360, 290);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 6 + i * 376, 44);
    lv_obj_set_style_bg_color(btn, i == 0 ? lv_color_hex(0x0075EE)
                                          : lv_color_hex(0x00897B),
                              LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 14, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, onCourse, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    lv_obj_t *ttl = lv_label_create(btn);
    lv_obj_set_style_text_font(ttl, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(ttl, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(ttl, TrainingTxt(c->title));
    lv_obj_align(ttl, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *sub = makeWrapLabel(btn, TrainingTxt(c->subtitle), 0, 90, 320,
                                  &lv_font_montserrat_14,
                                  lv_color_hex(0xF0F0F0));
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 90);

    char line[48];
    progressLine(line, sizeof(line), i);
    lv_obj_t *prog = lv_label_create(btn);
    lv_obj_set_style_text_font(prog, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(prog, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(prog, line);
    lv_obj_align(prog, LV_ALIGN_BOTTOM_MID, 0, -40);

    lv_obj_t *lessons = lv_label_create(btn);
    char n[40];
    snprintf(n, sizeof(n), "%u %s", (unsigned)c->lessonCount,
             TXT("lecciones", "lessons", "lecons"));
    lv_obj_set_style_text_font(lessons, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lessons, lv_color_hex(0xF0F0F0), 0);
    lv_label_set_text(lessons, n);
    lv_obj_align(lessons, LV_ALIGN_BOTTOM_MID, 0, -14);
  }

  const uint8_t certs = TrainingProgress_CertCount();
  if (certs > 0) {
    char t[40];
    snprintf(t, sizeof(t), "%s (%u)",
             TXT("CERTIFICADOS", "CERTIFICATES", "CERTIFICATS"), (unsigned)certs);
    lv_obj_t *b = makeBtn(s_content, t, onCerts, lv_color_hex(0x888888));
    lv_obj_set_size(b, 260, 46);
    lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 6, -6);
  }
}

void buildContinue() {
  makeTitle(TrainingTxt(s_course->title));
  const TrainingCourseProgress *p = TrainingProgress_Get(s_course->id);
  char line[48];
  progressLine(line, sizeof(line), s_course->id);
  char msg[240];
  snprintf(msg, sizeof(msg), "%s\n\n%s",
           TXT("Hay un curso empezado:", "There is a course in progress:",
               "Un cours est deja commence :"),
           line);
  (void)p;
  lv_obj_t *lbl = makeWrapLabel(s_content, msg, 0, 70, CARD_W - 20,
                                &lv_font_montserrat_20, lv_color_hex(0x0B2E4F));
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *cont = makeBtn(s_content, TXT("CONTINUAR", "CONTINUE", "CONTINUER"),
                           onContinue, lv_color_hex(0x00AA00));
  lv_obj_set_size(cont, 300, 60);
  lv_obj_align(cont, LV_ALIGN_CENTER, 0, 10);

  lv_obj_t *nw = makeBtn(s_content,
                         TXT("NUEVO ALUMNO (borra el progreso)",
                             "NEW STUDENT (clears progress)",
                             "NOUVEL ELEVE (efface la progression)"),
                         onNewStudent, lv_color_hex(0xE08800));
  lv_obj_set_size(nw, 420, 50);
  lv_obj_align(nw, LV_ALIGN_CENTER, 0, 90);

  lv_obj_t *back = makeBtn(s_content, TXT("VOLVER", "BACK", "RETOUR"),
                           onBackCourses, lv_color_hex(0x888888));
  lv_obj_set_size(back, 150, 46);
  lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 6, -6);
}

void buildName() {
  makeTitle(TXT("TU NOMBRE O INICIALES", "YOUR NAME OR INITIALS",
                "VOTRE NOM OU VOS INITIALES"));

  s_nameTa = lv_textarea_create(s_content);
  lv_obj_set_size(s_nameTa, 520, 52);
  lv_obj_align(s_nameTa, LV_ALIGN_TOP_MID, 0, 40);
  lv_textarea_set_one_line(s_nameTa, true);
  lv_textarea_set_max_length(s_nameTa, TRAINING_NAME_LEN - 1);
  lv_obj_set_style_text_font(s_nameTa, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_align(s_nameTa, LV_TEXT_ALIGN_CENTER, 0);
  lv_textarea_set_placeholder_text(s_nameTa, "...");

  lv_obj_t *hint = makeWrapLabel(
      s_content,
      TXT("Con este nombre se guardara tu progreso y tu certificado.",
          "Your progress and certificate will be saved under this name.",
          "Votre progression et votre certificat seront enregistres sous ce "
          "nom."),
      0, 98, CARD_W - 20, &lv_font_montserrat_14, lv_color_hex(0x666666));
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *back = makeBtn(s_content, TXT("VOLVER", "BACK", "RETOUR"),
                           onBackCourses, lv_color_hex(0x888888));
  lv_obj_set_size(back, 150, 46);
  lv_obj_align(back, LV_ALIGN_TOP_LEFT, 6, 126);

  lv_obj_t *cont = makeBtn(s_content, TXT("CONTINUAR", "CONTINUE", "CONTINUER"),
                           onNameContinue, lv_color_hex(0x00AA00));
  lv_obj_set_size(cont, 190, 46);
  lv_obj_align(cont, LV_ALIGN_TOP_RIGHT, -6, 126);

  lv_obj_t *kb = lv_btnmatrix_create(s_content);
  lv_btnmatrix_set_map(kb, KB_LETTERS_MAP);
  lv_btnmatrix_set_ctrl_map(kb, KB_LETTERS_CTRL);
  lv_obj_set_size(kb, 750, 250);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_obj_set_style_text_font(kb, &lv_font_montserrat_20, LV_PART_ITEMS);
  lv_obj_add_event_cb(kb, onKeyPress, LV_EVENT_VALUE_CHANGED, s_nameTa);
}

void buildLessons() {
  const TrainingCourseProgress *p = TrainingProgress_Get(s_course->id);
  char t[80];
  snprintf(t, sizeof(t), "%s - %s", TrainingTxt(s_course->title),
           (p && p->name[0]) ? p->name : "");
  makeTitle(t);

  lv_obj_t *list = lv_obj_create(s_content);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, CARD_W - 32, 340);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, LV_PART_MAIN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);

  for (uint8_t i = 0; i < s_course->lessonCount; i++) {
    const Lesson &l = s_course->lessons[i];
    const bool done = TrainingProgress_IsLessonDone(s_course->id, i);
    lv_obj_t *row = lv_btn_create(list);
    lv_obj_set_size(row, CARD_W - 52, 48);
    lv_obj_set_style_bg_color(row, done ? lv_color_hex(0x2E7D32)
                                        : lv_color_hex(0x0075EE),
                              LV_PART_MAIN);
    lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(row, onLesson, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    char label[96];
    snprintf(label, sizeof(label), "%u. %s%s", (unsigned)(i + 1),
             TrainingTxt(l.title),
             (l.flags & LESSON_INTERACTIVE)
                 ? TXT("  (practica)", "  (hands-on)", "  (pratique)")
                 : "");
    lv_obj_t *lbl = lv_label_create(row);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl, label);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *st = lv_label_create(row);
    lv_obj_set_style_text_font(st, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(st, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(st, done ? LV_SYMBOL_OK : LV_SYMBOL_RIGHT);
    lv_obj_align(st, LV_ALIGN_RIGHT_MID, -10, 0);
  }

  lv_obj_t *back = makeBtn(s_content, TXT("VOLVER", "BACK", "RETOUR"),
                           onBackCourses, lv_color_hex(0x888888));
  lv_obj_set_size(back, 150, 46);
  lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 6, -6);

  char line[48];
  progressLine(line, sizeof(line), s_course->id);
  lv_obj_t *prog = lv_label_create(s_content);
  lv_obj_set_style_text_font(prog, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(prog, lv_color_hex(0x0B2E4F), 0);
  lv_label_set_text(prog, line);
  lv_obj_align(prog, LV_ALIGN_BOTTOM_RIGHT, -10, -16);
}

void formatDate(char *out, size_t cap, uint32_t epoch) {
  if (epoch == 0) {
    snprintf(out, cap, "%s", TXT("sin hora", "no time", "sans heure"));
    return;
  }
  const time_t t = (time_t)(HMI_HasLocalTime() ? HMI_ToLocal(epoch) : epoch);
  struct tm tmv;
  gmtime_r(&t, &tmv);
  snprintf(out, cap, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1,
           tmv.tm_mday);
}

void buildCerts() {
  makeTitle(TXT("CERTIFICADOS", "CERTIFICATES", "CERTIFICATS"));
  lv_obj_t *list = lv_obj_create(s_content);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, CARD_W - 32, 340);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, LV_PART_MAIN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);

  const uint8_t n = TrainingProgress_CertCount();
  for (uint8_t i = 0; i < n; i++) {
    const TrainingCert *c = TrainingProgress_CertAt(i);
    if (!c) continue;
    const Course *course = Training_CourseByIndex(c->course);
    char date[16];
    formatDate(date, sizeof(date), c->epoch);
    char label[96];
    snprintf(label, sizeof(label), "%s - %s - %s", c->name,
             course ? TrainingTxt(course->title) : "?", date);
    lv_obj_t *row = lv_btn_create(list);
    lv_obj_set_size(row, CARD_W - 52, 48);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x2E7D32), LV_PART_MAIN);
    lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(row, onCert, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *lbl = lv_label_create(row);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl, label);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 10, 0);
  }

  lv_obj_t *back = makeBtn(s_content, TXT("VOLVER", "BACK", "RETOUR"),
                           onBackCourses, lv_color_hex(0x888888));
  lv_obj_set_size(back, 150, 46);
  lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 6, -6);
}

void buildCert() {
  const TrainingCert *c = TrainingProgress_CertAt(s_certIdx);
  if (!c) {
    showView(V_COURSES);
    return;
  }
  const Course *course = Training_CourseByIndex(c->course);
  const char *courseTitle = course ? TrainingTxt(course->title) : "?";
  makeTitle(TXT("CERTIFICADO", "CERTIFICATE", "CERTIFICAT"));

  char date[16];
  formatDate(date, sizeof(date), c->epoch);
  char subject[96];
  snprintf(subject, sizeof(subject), "IncuNest SN %04d - %s %s - %s",
           in3.serialNumber,
           TXT("Certificado", "Certificate", "Certificat"), courseTitle,
           c->name);
  char body[360];
  snprintf(body, sizeof(body),
           "%s\n%s: %s\n%s: %s\n%s: %u/%u\n%s: %u\n%s: %s\nIncuNest SN %04d, HMI %s\n",
           TXT("Certificado de formacion IncuNest",
               "IncuNest training certificate",
               "Certificat de formation IncuNest"),
           TXT("Alumno", "Student", "Eleve"), c->name,
           TXT("Curso", "Course", "Cours"), courseTitle,
           TXT("Lecciones superadas", "Lessons passed", "Lecons reussies"),
           (unsigned)c->lessons, (unsigned)c->lessons,
           TXT("Intentos fallidos en preguntas", "Failed quiz attempts",
               "Tentatives ratees aux questions"),
           (unsigned)c->attempts, TXT("Fecha", "Date", "Date"), date,
           in3.serialNumber, FWversion);

  lv_obj_t *qr = lv_qrcode_create(s_content, QR_SIZE, lv_color_hex(0x000000),
                                  lv_color_hex(0xFFFFFF));
  lv_obj_align(qr, LV_ALIGN_TOP_LEFT, 20, 50);
  lv_obj_set_style_border_color(qr, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(qr, 8, 0);
  const size_t n = mailto_build(s_mailto, sizeof(s_mailto), TRAINING_EMAIL,
                                subject, body);
  if (n > 0) lv_qrcode_update(qr, s_mailto, (uint32_t)n);

  char info[420];
  snprintf(info, sizeof(info), "%s\n\n%s\n\n%s %s",
           body,
           TXT("Escanea el QR con tu movil: se abre un correo con el "
               "certificado para el responsable de formacion.",
               "Scan the QR with your phone: an email with the certificate "
               "opens, addressed to the training lead.",
               "Scannez le QR avec votre telephone : un e-mail avec le "
               "certificat s'ouvre, adresse au responsable de formation."),
           TXT("Para:", "To:", "A :"), TRAINING_EMAIL);
  makeWrapLabel(s_content, info, 350, 50, CARD_W - 20 - 356,
                &lv_font_montserrat_14, lv_color_hex(0x0B2E4F));

  lv_obj_t *back = makeBtn(s_content, TXT("VOLVER", "BACK", "RETOUR"),
                           onBackCourses, lv_color_hex(0x888888));
  lv_obj_set_size(back, 150, 46);
  lv_obj_align(back, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
}

void showView(View v) {
  if (!s_content) return;
  lv_obj_clean(s_content);
  s_nameTa = nullptr;
  s_view = v;
  if ((v == V_CONTINUE || v == V_NAME || v == V_LESSONS) && !s_course) v = V_COURSES;
  switch (v) {
    case V_COURSES: buildCourses(); break;
    case V_CONTINUE: buildContinue(); break;
    case V_NAME: buildName(); break;
    case V_LESSONS: buildLessons(); break;
    case V_CERTS: buildCerts(); break;
    case V_CERT: buildCert(); break;
  }
}

}  // namespace

void TrainingSelector_Init(lv_obj_t *parent) {
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

void Training_OpenSelector(void) {
  s_course = nullptr;
  openAt(V_COURSES);
}

bool TrainingSelector_IsOpen(void) { return s_open; }

void TrainingSelector_Close(void) {
  if (s_open) closeDialog();
}

void TrainingSelector_Poll(void) {
  if (!s_open) return;
  // Mismo criterio que HelpDialog_Poll(): el selector tapa la pantalla
  // principal (y su icono de alarmas) y esta exento del auto-bloqueo, asi que
  // cede ante cualquier alarma, enlace perdido o apagado, y se cierra solo
  // tras HELP_IDLE_TIMEOUT_MS sin tocar.
  if (UI_IsAnyAlarmActive() || Display_IsBoardLinkLost() || g_pwrOffActive ||
      lv_disp_get_inactive_time(NULL) > HELP_IDLE_TIMEOUT_MS) {
    closeDialog();
  }
}

void TrainingSelector_OnLessonEnd(const Course *course, uint8_t lessonIdx,
                                  bool passed, uint16_t attempts) {
  if (!course) return;
  s_course = course;
  if (passed) {
    TrainingProgress_MarkLesson(course->id, lessonIdx, attempts);
    bool all = true;
    for (uint8_t i = 0; i < course->lessonCount; i++) {
      if (!TrainingProgress_IsLessonDone(course->id, i)) {
        all = false;
        break;
      }
    }
    if (all) {
      TrainingProgress_Certify(course->id, course->lessonCount);
      s_certIdx = 0;
      UI_ShowToast(TXT("Curso superado. Enhorabuena!", "Course passed. Well done!",
                       "Cours reussi. Felicitations !"),
                   4000);
      openAt(V_CERT);
      return;
    }
    UI_ShowToast(TXT("Leccion superada", "Lesson passed", "Lecon reussie"), 3000);
  } else {
    UI_ShowToast(TXT("Leccion no completada", "Lesson not completed",
                     "Lecon non terminee"),
                 3000);
  }
  openAt(V_LESSONS);
}

#pragma once
// Cursos de formacion (spec hmi-training-courses): selector de cursos, motor
// de lecciones interactivas, progreso y certificado.
//
// Dos overlays:
//   - Selector (cursos, alumno, lecciones, certificados): modal sobre
//     ui_ScreenMain con el patron de TimeDialog/HelpDialog.
//   - Motor de lecciones: overlay en lv_layer_top() (sobrevive al cambio de
//     pantalla) con sombras, recuadro, bocadillo y la franja inferior de
//     "MODO FORMACION". Raiz transparente y NO clicable: en los pasos "hacer"
//     el hueco del recuadro deja pasar el toque al control real
//     (lv_indev_search_obj busca en los hijos por area y solo devuelve la
//     raiz si es clicable); las sombras clicables se tragan el resto.
#include <lvgl.h>

#include "ui/training/lesson_types.h"

// --- Init (UITask, en el orden de creacion de lv_layer_top()) --------------
// Crea el overlay del motor (oculto). Llamar donde antes se llamaba
// HelpTour_Init(): ANTES de alarm_banner_init() para quedar por debajo del
// banner y del icono AUDIO PAUSED.
void Training_Init(void);
// Crea el selector (oculto), parent ui_ScreenMain. Junto a HelpDialog_Init.
void TrainingSelector_Init(lv_obj_t *parent);

// --- Entrada desde el menu de ayuda -----------------------------------------
void Training_OpenSelector(void);

// --- Estado para UITask ------------------------------------------------------
// Verdadero con el selector o una leccion en pantalla: exencion del
// auto-bloqueo (con tope: HELP_IDLE_TIMEOUT_MS).
bool Training_IsOpen(void);
// Una vez por vuelta de UI_Task, bajo LVGL_Lock(), despues de
// Training_ServiceReplies() y de los _Poll() de los asistentes: evalua el
// objetivo del paso, aborta ante alarma / enlace / apagado / inactividad.
void Training_Poll(void);

// --- Motor (lo usa el selector) ----------------------------------------------
// Arranca una leccion. Si es interactiva y el gate clinico falla, se ofrece en
// demostracion (no cuenta como superada). El selector se oculta; al terminar
// o abortar, el motor llama a TrainingSelector_OnLessonEnd().
void Training_StartLesson(const Course *course, uint8_t lessonIdx);
// Cierra la leccion en curso (SALIR, alarma, inactividad): restaura estado,
// sale del modo formacion, vuelve a ui_ScreenMain.
void Training_AbortLesson(void);

// Callback del motor hacia el selector. passed=false en aborto o demostracion.
void TrainingSelector_OnLessonEnd(const Course *course, uint8_t lessonIdx,
                                  bool passed, uint16_t attempts);
bool TrainingSelector_IsOpen(void);

// Helper de texto compartido por los ficheros de training/.
const char *TrainingTxt(const Txt3 &t);

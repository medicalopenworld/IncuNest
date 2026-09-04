// Curso Tecnico (spec hmi-training-courses). Fase 1: solo T0 intro, para que
// el selector muestre los dos cursos. El resto llega en la fase 3 (tasks.md,
// seccion 9).
#include "ui/training/lessons.h"

extern const Lesson LESSON_INTRO;  // lessons_intro.cpp

namespace {

const Lesson TECH_LESSONS[] = {
    LESSON_INTRO,
};

}  // namespace

const Course COURSE_TECH = {
    TRAINING_COURSE_TECH,
    T3("Tecnico", "Technician", "Technicien"),
    T3("Configuracion: informacion, WiFi, idioma, hora, alarmas tecnicas y "
       "actualizacion.",
       "Setup: information, WiFi, language, time, technical alarms and "
       "updates.",
       "Configuration : informations, WiFi, langue, heure, alarmes techniques "
       "et mise a jour."),
    TECH_LESSONS,
    (uint8_t)(sizeof(TECH_LESSONS) / sizeof(TECH_LESSONS[0])),
};

const Course *Training_CourseByIndex(uint8_t idx) {
  switch (idx) {
    case TRAINING_COURSE_NURSE: return &COURSE_NURSE;
    case TRAINING_COURSE_TECH: return &COURSE_TECH;
    default: return nullptr;
  }
}

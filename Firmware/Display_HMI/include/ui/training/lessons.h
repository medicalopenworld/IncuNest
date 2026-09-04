#pragma once
// Tablas de cursos (spec hmi-training-courses). Cada curso vive en su fichero
// lessons_*.cpp; el selector solo necesita los punteros.
#include "ui/training/lesson_types.h"

#define TRAINING_COURSE_NURSE 0
#define TRAINING_COURSE_TECH 1

// Leccion 0 compartida (lessons_intro.cpp). Declarada aqui para que su
// definicion tenga enlace externo (un `const` a nivel de namespace es interno
// por defecto en C++).
extern const Lesson LESSON_INTRO;
extern const Course COURSE_NURSE;
extern const Course COURSE_TECH;

// Curso por indice de NVS (TRAINING_COURSE_*), nullptr si no existe.
const Course *Training_CourseByIndex(uint8_t idx);

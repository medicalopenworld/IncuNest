#pragma once
// Tipos declarativos del motor de lecciones (spec hmi-training-courses,
// design.md decision 3). Una leccion es una tabla estatica de pasos; anadir
// una leccion es anadir una tabla en lessons_*.cpp, sin tocar el motor.
#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

// Texto en los tres idiomas de la interfaz. ASCII sin acentos (las fuentes
// Montserrat compiladas solo traen 32-126). Cuando aterrice el catalogo i18n
// (feat/hmi-i18n-catalogo) estas ternas migran alli.
struct Txt3 {
  const char *es;
  const char *en;
  const char *fr;
};

enum StepKind : uint8_t {
  STEP_EXPLAIN = 0,  // resaltar y leer; SIGUIENTE avanza
  STEP_DO,           // el alumno toca el control real; avanza al cumplir goal()
  STEP_QUIZ,         // pregunta con tres opciones; reintento hasta acertar
};

// Flags de paso.
enum : uint8_t {
  // STEP_DO sin sombras ni recuadro: el alumno interactua con un asistente
  // completo y la instruccion va en la franja inferior. goal() decide.
  STEP_FREE = 1 << 0,
};

struct Quiz {
  Txt3 options[3];
  uint8_t correct;   // indice 0..2
  Txt3 explain;      // por que la correcta es la correcta (tras fallo)
};

typedef bool (*GoalFn)(void);
typedef void (*EnterFn)(void);

struct Step {
  StepKind kind;
  lv_obj_t **target;   // control a resaltar / dejar tocar (nullptr = ninguno)
  lv_obj_t **screen;   // pantalla que debe estar cargada al mostrar el paso
  Txt3 text;           // explicacion, instruccion o pregunta
  GoalFn goal;         // STEP_DO: verdadero cuando esta hecho
  EnterFn onEnter;     // opcional: captura una referencia (p. ej. consigna inicial)
  uint8_t flags;
  const Quiz *quiz;    // STEP_QUIZ
};

// Flags de leccion.
enum : uint8_t {
  // Cambia estado (consignas, toggles, asistentes): exige gate clinico y
  // corre en modo formacion. Sin este flag la leccion no toca nada y no lo
  // necesita (intro, bloqueo, tendencia, soporte).
  LESSON_INTERACTIVE = 1 << 0,
};

struct Lesson {
  uint8_t id;
  Txt3 title;
  const Step *steps;
  uint8_t stepCount;
  uint8_t flags;
};

struct Course {
  uint8_t id;          // indice en NVS (0 = enfermeria, 1 = tecnico)
  Txt3 title;
  Txt3 subtitle;
  const Lesson *lessons;
  uint8_t lessonCount;
};

// Atajos para escribir tablas.
#define T3(es, en, fr) Txt3{es, en, fr}
#define EXPLAIN(target, screen, es, en, fr) \
  Step{STEP_EXPLAIN, target, screen, T3(es, en, fr), nullptr, nullptr, 0, nullptr}
#define DO(target, screen, goal, es, en, fr) \
  Step{STEP_DO, target, screen, T3(es, en, fr), goal, nullptr, 0, nullptr}
#define DO_ENTER(target, screen, goal, onEnter, es, en, fr) \
  Step{STEP_DO, target, screen, T3(es, en, fr), goal, onEnter, 0, nullptr}
#define FREE(screen, goal, es, en, fr) \
  Step{STEP_DO, nullptr, screen, T3(es, en, fr), goal, nullptr, STEP_FREE, nullptr}
#define FREE_ENTER(screen, goal, onEnter, es, en, fr) \
  Step{STEP_DO, nullptr, screen, T3(es, en, fr), goal, onEnter, STEP_FREE, nullptr}
#define QUIZ(screen, quizPtr, es, en, fr) \
  Step{STEP_QUIZ, nullptr, screen, T3(es, en, fr), nullptr, nullptr, 0, quizPtr}

#pragma once
// Tipos declarativos del motor de lecciones (spec hmi-training-courses,
// design.md decision 3). Una leccion es una tabla estatica de pasos; anadir
// una leccion es anadir una tabla en lessons_*.cpp, sin tocar el motor.
#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

// Texto de una leccion en los idiomas de la interfaz. ASCII sin acentos (las
// fuentes Montserrat compiladas solo traen 32-126).
//
// Por que NO vive en el catalogo (`ui/i18n_strings.def`), a diferencia del
// resto de la interfaz: esto no son cadenas de interfaz sino CONTENIDO. Son
// 218 parrafos que lee un unico modulo, y cada uno solo tiene sentido junto al
// paso que explica. Meterlos en el catalogo doblaria su tamano con ids
// (STR_LESSON_NURSE_07_TEXT...) que no aportan nada y obligaria a saltar de
// fichero para leer una leccion. El comentario original de este struct decia
// que migrarian al catalogo; al llegar ahi con 218 unidades, la decision se
// revisa aqui a proposito.
//
// El precio, y hay que saberlo: anadir un idioma ya no es solo una columna en
// el `.def`, son dos sitios — el `.def` y estas tablas. Lo dice tambien
// `ui/i18n.h`.
struct LessonTxt {
  const char *es;
  const char *en;
  const char *fr;
  const char *pt;
};

// Todas las celdas de una tabla de lecciones deben ser ASCII 32-126: las
// Montserrat cargadas no tienen mas glifos y una letra acentuada se pinta como
// caja vacia. Cada `lessons_*.cpp` remata su tabla con
// LESSON_TABLE_IS_ASCII(...) para que un descuido sea error de compilacion, no
// un hallazgo al mirar la pantalla en ese idioma.
constexpr bool lesson_txt_is_ascii(const char *s) {
  return s == nullptr
             ? true
             : (*s == '\0'
                    ? true
                    : ((*s == '\n' || (*s >= 0x20 && *s <= 0x7E)) &&
                       lesson_txt_is_ascii(s + 1)));
}

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
  LessonTxt options[3];
  uint8_t correct;   // indice 0..2
  LessonTxt explain;      // por que la correcta es la correcta (tras fallo)
};

typedef bool (*GoalFn)(void);
typedef void (*EnterFn)(void);

struct Step {
  StepKind kind;
  lv_obj_t **target;   // control a resaltar / dejar tocar (nullptr = ninguno)
  lv_obj_t **screen;   // pantalla que debe estar cargada al mostrar el paso
  LessonTxt text;           // explicacion, instruccion o pregunta
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

typedef bool (*AvailFn)(void);

struct Lesson {
  uint8_t id;
  LessonTxt title;
  const Step *steps;
  uint8_t stepCount;
  uint8_t flags;
  // Opcional: la leccion solo se lista (y solo cuenta para el certificado)
  // si devuelve verdadero. Para funciones que se habilitan en Ajustes
  // (humedad, modo piel): si el equipo no las usa, no se ensenan.
  AvailFn available;
};

// Verdadero si la leccion esta disponible ahora.
static inline bool Lesson_IsAvailable(const Lesson &l) {
  return l.available == nullptr || l.available();
}

struct Course {
  uint8_t id;          // indice en NVS (0 = enfermeria, 1 = tecnico)
  LessonTxt title;
  LessonTxt subtitle;
  const Lesson *lessons;
  uint8_t lessonCount;
};

// Atajos para escribir tablas. Las cuatro columnas se nombran una a una (y no
// con `...`) para que a una fila a la que le falte un idioma no se le rellene
// el hueco con nada: falta un argumento y no compila. Mismo criterio que la
// macro del catalogo en i18n.cpp.
#define T4(es, en, fr, pt) LessonTxt{es, en, fr, pt}
#define EXPLAIN(target, screen, es, en, fr, pt) \
  Step{STEP_EXPLAIN, target, screen, T4(es, en, fr, pt), nullptr, nullptr, 0, nullptr}
#define DO(target, screen, goal, es, en, fr, pt) \
  Step{STEP_DO, target, screen, T4(es, en, fr, pt), goal, nullptr, 0, nullptr}
#define DO_ENTER(target, screen, goal, onEnter, es, en, fr, pt) \
  Step{STEP_DO, target, screen, T4(es, en, fr, pt), goal, onEnter, 0, nullptr}
#define FREE(screen, goal, es, en, fr, pt) \
  Step{STEP_DO, nullptr, screen, T4(es, en, fr, pt), goal, nullptr, STEP_FREE, nullptr}
#define FREE_ENTER(screen, goal, onEnter, es, en, fr, pt) \
  Step{STEP_DO, nullptr, screen, T4(es, en, fr, pt), goal, onEnter, STEP_FREE, nullptr}
#define QUIZ(screen, quizPtr, es, en, fr, pt) \
  Step{STEP_QUIZ, nullptr, screen, T4(es, en, fr, pt), nullptr, nullptr, 0, quizPtr}

// Guarda ASCII de una tabla de pasos. Se pone una vez por tabla, justo debajo.
#define LESSON_TABLE_IS_ASCII(tbl)                                        \
  static_assert(                                                          \
      [] {                                                                \
        for (const Step &st : tbl) {                                      \
          if (!lesson_txt_is_ascii(st.text.es) ||                         \
              !lesson_txt_is_ascii(st.text.en) ||                         \
              !lesson_txt_is_ascii(st.text.fr) ||                         \
              !lesson_txt_is_ascii(st.text.pt)) {                         \
            return false;                                                 \
          }                                                               \
        }                                                                 \
        return true;                                                      \
      }(),                                                                \
      #tbl " tiene un caracter fuera de ASCII 32-126: las fuentes "       \
           "Montserrat cargadas no pueden pintarlo")

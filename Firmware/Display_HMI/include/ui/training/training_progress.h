#pragma once
// Progreso de los cursos y certificados (spec hmi-training-courses, design
// decision 5). En RAM siempre; en NVS (namespace hmi_train) via el bucle de
// UI, fuera de LVGL_Lock(), con el mismo patron que eepromDirty.
#include <stdbool.h>
#include <stdint.h>

#define TRAINING_COURSES 2
#define TRAINING_NAME_LEN 24   // igual que BABY_NAME_LEN_LOCAL
#define TRAINING_CERT_RING 16

struct TrainingCourseProgress {
  char name[TRAINING_NAME_LEN];  // alumno en curso ("" = ninguno)
  uint32_t doneMask;             // bit i = leccion i superada (interactiva)
  uint16_t attempts;             // intentos fallidos acumulados en preguntas
};

struct TrainingCert {
  char name[TRAINING_NAME_LEN];
  uint8_t course;
  uint8_t lessons;
  uint16_t attempts;
  uint32_t epoch;  // UTC; 0 si el reloj no estaba sincronizado
};

// Carga desde NVS. Llamar una vez al arrancar (antes de la UI, o en su init).
void TrainingProgress_Load(void);

const TrainingCourseProgress *TrainingProgress_Get(uint8_t course);
// Empieza (o reinicia) el curso a nombre de `name`: borra progreso.
void TrainingProgress_StartCourse(uint8_t course, const char *name);
void TrainingProgress_MarkLesson(uint8_t course, uint8_t lesson,
                                 uint16_t attempts);
bool TrainingProgress_IsLessonDone(uint8_t course, uint8_t lesson);
// Registra el certificado (anillo de TRAINING_CERT_RING) y limpia el alumno
// en curso de ese curso.
void TrainingProgress_Certify(uint8_t course, uint8_t lessonCount);

uint8_t TrainingProgress_CertCount(void);
// 0 = el mas reciente.
const TrainingCert *TrainingProgress_CertAt(uint8_t idx);

// --- Persistencia (UITask) -----------------------------------------------------
// Verdadero (y lo pone a falso) si hay cambios sin escribir. Decidir bajo
// LVGL_Lock(), escribir fuera con TrainingProgress_Flush().
bool TrainingProgress_TakeDirty(void);
void TrainingProgress_Flush(void);

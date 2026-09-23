#include "ui/training/training_progress.h"

#include <Preferences.h>

#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "EEPROM_defines.h"
#include "esp_log.h"

static const char *TAG = "TrainProg";

namespace {

TrainingCourseProgress s_course[TRAINING_COURSES];
TrainingCert s_cert[TRAINING_CERT_RING];
uint8_t s_certCount = 0;   // cuantos validos (<= RING)
uint8_t s_certNext = 0;    // siguiente hueco del anillo
volatile bool s_dirty = false;

void keyCourse(char *out, size_t cap, uint8_t course, const char *field) {
  snprintf(out, cap, "c%u_%s", (unsigned)course, field);
}

void keyCert(char *out, size_t cap, uint8_t slot) {
  snprintf(out, cap, "cert_%u", (unsigned)slot);
}

}  // namespace

void TrainingProgress_Load(void) {
  memset(s_course, 0, sizeof(s_course));
  memset(s_cert, 0, sizeof(s_cert));
  Preferences p;
  if (!p.begin(HMI_NS_TRAIN, true)) {
    // Namespace aun no existe: sin progreso. Se creara al primer Flush().
    s_certCount = 0;
    s_certNext = 0;
    return;
  }
  char k[16];
  for (uint8_t c = 0; c < TRAINING_COURSES; c++) {
    keyCourse(k, sizeof(k), c, "name");
    p.getString(k, s_course[c].name, sizeof(s_course[c].name));
    keyCourse(k, sizeof(k), c, "done");
    s_course[c].doneMask = p.getUInt(k, 0);
    keyCourse(k, sizeof(k), c, "att");
    s_course[c].attempts = p.getUShort(k, 0);
  }
  s_certCount = p.getUChar(HMI_KEY_TRAIN_CERT_CNT, 0);
  if (s_certCount > TRAINING_CERT_RING) s_certCount = TRAINING_CERT_RING;
  s_certNext = p.getUChar(HMI_KEY_TRAIN_CERT_NEXT, 0) % TRAINING_CERT_RING;
  for (uint8_t i = 0; i < TRAINING_CERT_RING; i++) {
    keyCert(k, sizeof(k), i);
    if (p.getBytesLength(k) == sizeof(TrainingCert)) {
      p.getBytes(k, &s_cert[i], sizeof(TrainingCert));
      // Un blob corrupto del tamano correcto no debe leerse mas alla del campo.
      s_cert[i].name[TRAINING_NAME_LEN - 1] = '\0';
    }
  }
  p.end();
  ESP_LOGI(TAG, "progreso cargado: %u certificados", (unsigned)s_certCount);
}

const TrainingCourseProgress *TrainingProgress_Get(uint8_t course) {
  if (course >= TRAINING_COURSES) return nullptr;
  return &s_course[course];
}

void TrainingProgress_StartCourse(uint8_t course, const char *name) {
  if (course >= TRAINING_COURSES) return;
  strncpy(s_course[course].name, name ? name : "", TRAINING_NAME_LEN - 1);
  s_course[course].name[TRAINING_NAME_LEN - 1] = '\0';
  s_course[course].doneMask = 0;
  s_course[course].attempts = 0;
  s_dirty = true;
}

void TrainingProgress_MarkLesson(uint8_t course, uint8_t lesson,
                                 uint16_t attempts) {
  if (course >= TRAINING_COURSES || lesson >= 32) return;
  s_course[course].doneMask |= (1u << lesson);
  s_course[course].attempts += attempts;
  s_dirty = true;
}

bool TrainingProgress_IsLessonDone(uint8_t course, uint8_t lesson) {
  if (course >= TRAINING_COURSES || lesson >= 32) return false;
  return (s_course[course].doneMask >> lesson) & 1u;
}

void TrainingProgress_Certify(uint8_t course, uint8_t lessonCount) {
  if (course >= TRAINING_COURSES) return;
  TrainingCert &c = s_cert[s_certNext];
  memcpy(c.name, s_course[course].name, TRAINING_NAME_LEN);
  c.name[TRAINING_NAME_LEN - 1] = '\0';
  c.course = course;
  c.lessons = lessonCount;
  c.attempts = s_course[course].attempts;
  c.epoch = HMI_GetEpochNow();
  s_certNext = (s_certNext + 1) % TRAINING_CERT_RING;
  if (s_certCount < TRAINING_CERT_RING) s_certCount++;
  // El alumno ya tiene su certificado: el curso queda libre para el siguiente.
  s_course[course].name[0] = '\0';
  s_course[course].doneMask = 0;
  s_course[course].attempts = 0;
  s_dirty = true;
}

uint8_t TrainingProgress_CertCount(void) { return s_certCount; }

const TrainingCert *TrainingProgress_CertAt(uint8_t idx) {
  if (idx >= s_certCount) return nullptr;
  // idx 0 = el ultimo escrito (s_certNext - 1).
  const int slot = ((int)s_certNext - 1 - (int)idx + 2 * TRAINING_CERT_RING) %
                   TRAINING_CERT_RING;
  return &s_cert[slot];
}

bool TrainingProgress_TakeDirty(void) {
  if (!s_dirty) return false;
  s_dirty = false;
  return true;
}

void TrainingProgress_Flush(void) {
  Preferences p;
  if (!p.begin(HMI_NS_TRAIN, false)) {
    ESP_LOGE(TAG, "no se pudo abrir el namespace %s", HMI_NS_TRAIN);
    return;
  }
  char k[16];
  for (uint8_t c = 0; c < TRAINING_COURSES; c++) {
    keyCourse(k, sizeof(k), c, "name");
    p.putString(k, s_course[c].name);
    keyCourse(k, sizeof(k), c, "done");
    p.putUInt(k, s_course[c].doneMask);
    keyCourse(k, sizeof(k), c, "att");
    p.putUShort(k, s_course[c].attempts);
  }
  p.putUChar(HMI_KEY_TRAIN_CERT_CNT, s_certCount);
  p.putUChar(HMI_KEY_TRAIN_CERT_NEXT, s_certNext);
  for (uint8_t i = 0; i < s_certCount && i < TRAINING_CERT_RING; i++) {
    // Solo los huecos ocupados; NVS no reescribe un blob identico.
    const int slot = ((int)s_certNext - 1 - (int)i + 2 * TRAINING_CERT_RING) %
                     TRAINING_CERT_RING;
    keyCert(k, sizeof(k), (uint8_t)slot);
    p.putBytes(k, &s_cert[slot], sizeof(TrainingCert));
  }
  p.end();
  ESP_LOGI(TAG, "progreso guardado");
}

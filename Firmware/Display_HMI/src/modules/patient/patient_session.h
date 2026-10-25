#pragma once
// Identidad y terapia acumulada del bebe al mando: logica PURA.
//
// Sin Arduino, sin LVGL, sin reloj propio (el tiempo entra por parametro), para
// poder correr en el entorno native del HMI. El estado vivo, la NVS y los
// widgets viven fuera: state/patient_state.cpp y ui/*.
//
// Spec: openspec/changes/hmi-patient-identity.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// --------------------------------------------------------------------------
// Edad
// --------------------------------------------------------------------------
// La motherBoard no manda fecha de nacimiento: manda `ageDays` en el instante
// de CTRL,PROFILE_RANGE. El HMI guarda (ageDays, epochCaptura) y suma los dias
// enteros transcurridos desde entonces. El error maximo es de +-1 dia respecto
// al cambio de dia de la placa, porque se cuenta desde la captura y no desde
// el ingreso.
struct PatientAge {
  bool hasLifeDays;    // false si la edad no se conoce
  uint32_t lifeDays;   // dias de vida
  bool hasCorrected;   // false sin edad o sin semanas de gestacion
  uint32_t corrWeeks;  // edad gestacional corregida: semanas...
  uint8_t corrDays;    // ...y dias (0-6)
};

// `captureEpoch` y `nowEpoch` son epoch UTC; 0 = "sin hora". Si falta alguno,
// o si la hora fue hacia atras, no se suma nada: se devuelve el valor
// capturado, que es un limite inferior y nunca un dato inventado.
PatientAge patient_age_compute(uint8_t gestWeeks, bool ageKnown,
                               uint16_t ageDaysAtCapture,
                               uint32_t captureEpoch, uint32_t nowEpoch);

// --------------------------------------------------------------------------
// Formato
// --------------------------------------------------------------------------
// "3h 20m", "45m", "3h" (horas exactas), "0m". Devuelve los caracteres
// escritos sin contar el NUL; siempre termina en NUL si len > 0.
size_t patient_fmt_minutes(uint32_t minutes, char *out, size_t len);

// --------------------------------------------------------------------------
// Acumulador de terapia por bebe
// --------------------------------------------------------------------------
// Misma regla que la motherBoard (baby_profile, CommTask.cpp): termo con
// act 1 o 3, humedad con act 2 o 3, foto con photo != 0.
enum {
  PATIENT_ACT_NONE = 0,
  PATIENT_ACT_TEMPERATURE = 1,
  PATIENT_ACT_HUMIDITY = 2,
  PATIENT_ACT_TEMP_AND_HUMIDITY = 3,
};

// Una pasada acredita como mucho esto: un atasco de la UI no se convierte en
// horas inventadas.
constexpr uint32_t PATIENT_TICK_MAX_CREDIT_MS = 5000u;
// Cadencia maxima de escritura a NVS del acumulado.
constexpr uint32_t PATIENT_FLUSH_INTERVAL_MS = 60000u;

struct PatientMinutes {
  uint32_t photo;
  uint32_t thermo;
  uint32_t humidity;
};

struct PatientAccum {
  uint32_t seq;  // bebe al que se acredita; 0 = nadie
  uint32_t photoSec, thermoSec, humiditySec;
  uint32_t remMs;  // resto sub-segundo, compartido por los tres contadores
  uint32_t lastTickMs;
  bool hasTick;
  bool photoOn, thermoOn, humidityOn;  // estado en la ultima pasada
  bool dirty;                          // cambio sin volcar a NVS
  bool stopEdge;                       // todas se pararon: volcar ya
  uint32_t lastFlushMs;
};

// Pone todo a cero y sin bebe.
void patient_accum_init(PatientAccum *a);

// Empieza a acreditar a `seq` con la siembra dada (minutos, como llegan en
// PROFILE_LIST); `seed` puede ser nullptr (= 0). seq 0 equivale a init().
void patient_accum_begin(PatientAccum *a, uint32_t seq,
                         const PatientMinutes *seed);

// Una pasada (~1 Hz). `act`/`photo` salen del CTRL,STATE real; `linkOk` es
// false si la placa lleva callada demasiado (el CTRL,STATE guardado esta
// obsoleto). Devuelve true si algun minuto MOSTRADO cambio.
bool patient_accum_tick(PatientAccum *a, uint32_t nowMs, uint8_t act,
                        bool photo, bool linkOk);

PatientMinutes patient_accum_minutes(const PatientAccum *a);

// Una PROFILE_LIST de la placa. Solo vale si `seq` es el del acumulador. Un
// contador que corre en el HMI toma el mayor de los dos (la placa vuelca cada
// 10 min, asi que su cifra es una cota inferior); uno parado toma el de la
// placa. Devuelve true si algun minuto mostrado cambio.
bool patient_accum_resync(PatientAccum *a, uint32_t seq,
                          const PatientMinutes *board);

// Hay que volcar a NVS: cambio pendiente y (paso el intervalo o se paro todo).
bool patient_accum_flush_due(const PatientAccum *a, uint32_t nowMs);
void patient_accum_flush_done(PatientAccum *a, uint32_t nowMs);

// --------------------------------------------------------------------------
// Registros de NVS
// --------------------------------------------------------------------------
// Serializados a mano, little-endian, en bytes: un memcpy de la struct
// arrastraria el relleno y haria el formato dependiente del compilador. Cada
// registro es UN blob: la NVS no es transaccional, y con claves sueltas un
// corte dejaria el nombre de un bebe con los minutos de otro.
constexpr uint8_t PATIENT_RECORD_VERSION = 1;
constexpr size_t PATIENT_NAME_LEN = 24;
constexpr size_t PATIENT_ID_RECORD_SIZE = 40;
constexpr size_t PATIENT_THER_RECORD_SIZE = 20;

struct PatientIdentity {
  uint32_t seq;
  char name[PATIENT_NAME_LEN];
  uint8_t gestWeeks;
  bool ageKnown;
  uint16_t ageDays;
  uint32_t captureEpoch;
};

// Escribe PATIENT_ID_RECORD_SIZE bytes en `out`. false si seq == 0 o args nulos.
bool patient_identity_encode(const PatientIdentity *id, uint8_t *out,
                             size_t outLen);
// Valida y decodifica. false (y `id` intacto) con tamano distinto, version
// desconocida, seq == 0 o nombre sin NUL dentro de los 24 bytes.
bool patient_identity_decode(const uint8_t *in, size_t len, PatientIdentity *id);

struct PatientTherapyRecord {
  uint32_t seq;
  uint32_t photoSec, thermoSec, humiditySec;
};
bool patient_therapy_encode(const PatientAccum *a, uint8_t *out, size_t outLen);
bool patient_therapy_decode(const uint8_t *in, size_t len,
                            PatientTherapyRecord *rec);
// Restaura el acumulador desde un registro, solo si es del `expectedSeq`.
bool patient_accum_restore(PatientAccum *a, const PatientTherapyRecord *rec,
                           uint32_t expectedSeq);

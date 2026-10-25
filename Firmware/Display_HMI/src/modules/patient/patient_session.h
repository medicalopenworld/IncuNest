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

// `captureEpoch` y `nowEpoch` son epoch UTC; 0 = "sin hora". La edad es
// DESCONOCIDA (hasLifeDays = hasCorrected = false) si no hubo hora al capturar,
// si falta ahora, o si han pasado mas de PATIENT_AGE_MAX_ELAPSED_DAYS: sin hora
// no se puede afirmar cuantos dias han pasado, y un contador congelado en el
// valor de hace horas es peor que ninguno. Si la hora fue hacia atras (un
// ajuste de reloj) no se suma nada.
constexpr uint32_t PATIENT_AGE_MAX_ELAPSED_DAYS = 365u;
PatientAge patient_age_compute(uint8_t gestWeeks, bool ageKnown,
                               uint16_t ageDaysAtCapture,
                               uint32_t captureEpoch, uint32_t nowEpoch);

// Dias de vida para el detalle del boton "Bebes" (que mide 110 px): "5", "99",
// "99+" a partir de PATIENT_LIFE_DAYS_CAP. El formato localizado pone la "d"/"j"
// detras.
constexpr uint32_t PATIENT_LIFE_DAYS_CAP = 99u;
size_t patient_fmt_life_days(uint32_t days, char *out, size_t len);

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
  // false = no se sabe cuantos minutos lleva este bebe: no hay siembra, ni
  // registro de NVS valido, ni lista de la placa que los diga. Un contador que
  // empezase en 0 se mostraria como el total real. Mientras no se conozcan, no
  // se cuenta ni se muestra; la primera lista (resync) los fija.
  bool known;
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
//
// pat_id v2 anade el `sn` de la placa: sin el, un seq solo identifica a un bebe
// DENTRO de una placa y dos placas colisionan. La v1 no se puede atar a nadie y
// se descarta.
constexpr uint8_t PATIENT_ID_RECORD_VERSION = 2;
constexpr uint8_t PATIENT_THER_RECORD_VERSION = 1;
constexpr size_t PATIENT_NAME_LEN = 24;
constexpr size_t PATIENT_ID_RECORD_SIZE = 44;
constexpr size_t PATIENT_ID_RECORD_V1_SIZE = 40;  // solo para reconocerlo
constexpr size_t PATIENT_THER_RECORD_SIZE = 20;
// Semanas de gestacion admitidas al decodificar: 0 (no informadas) o 20-44. El
// asistente pide 20-40; el margen es para lo que ya viva en la placa.
constexpr uint8_t PATIENT_GEST_MIN = 20;
constexpr uint8_t PATIENT_GEST_MAX = 44;

struct PatientIdentity {
  uint32_t seq;
  char name[PATIENT_NAME_LEN];
  uint8_t gestWeeks;
  bool ageKnown;
  uint16_t ageDays;
  uint32_t captureEpoch;
  uint32_t boardSn;  // `sn` de la placa a la que pertenece el seq
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

// --------------------------------------------------------------------------
// Confirmacion de la identidad con la placa
// --------------------------------------------------------------------------
// Una identidad restaurada de la NVS nace SIN CONFIRMAR: el HMI solo sabe lo
// que vio, y puede estar enfrente de otra placa, de una NVS borrada o de un
// alta hecha por otra via. La placa si sabe quien esta al mando (activeSeq,
// campo 24 de CTRL,STATE). Estas funciones son la decision; el pegamento (leer
// ctrl_state_msg, pedir la lista, pintar) esta en state/patient_state.cpp.
struct PatientBoardView {
  bool stateSeen;        // ya llego algun CTRL,STATE de la placa
  bool sendsActiveSeq;   // la linea traia el campo 24 (placa nueva)
  uint32_t activeSeq;    // solo vale si sendsActiveSeq
  bool therapyOn;        // la placa dice act != 0 o photo != 0
  uint32_t sn;           // sn de la placa conectada
};
enum PatientConfirmAction {
  PATIENT_CONFIRM_WAIT = 0,  // sin decision todavia
  PATIENT_CONFIRM_CONFIRM,   // la placa confirma este bebe
  PATIENT_CONFIRM_CLEAR,     // la placa lo contradice: borrar
};
// Tabla de decision para una identidad restaurada (ver design.md D2). Con una
// placa que no manda activeSeq nunca decide por si sola: WAIT (se contrasta
// con la lista, patient_list_has_baby).
PatientConfirmAction patient_confirm_decide(uint32_t identitySeq,
                                            uint32_t identitySn,
                                            const PatientBoardView *board);

// Un renglon de CTRL,PROFILE_LIST, sin depender del tipo de CommTask.h.
struct PatientListEntry {
  uint32_t seq;
  char name[PATIENT_NAME_LEN];
  uint8_t gestWeeks;
};
// El bebe esta en la lista con el MISMO seq, nombre y semanas. Cualquier
// diferencia (mismo seq con otro nombre = otro bebe) es "no esta".
bool patient_list_has_baby(const PatientIdentity *id,
                           const PatientListEntry *entries, int count);

// Mismo bebe que el recordado: seq, nombre y semanas iguales. Con el mismo seq
// y otro nombre la placa reasigno el seq: es otro bebe, y su edad y sus
// acumulados no valen.
bool patient_same_baby(const PatientIdentity *id, uint32_t seq,
                       const char *name, uint8_t gestWeeks);

// Acreditar minutos solo si la placa dice que ESE bebe esta al mando (la placa
// acredita al activeSeq y, con 0, a nadie); sin dato de la placa, solo con la
// identidad confirmada.
bool patient_credit_allowed(uint32_t accSeq, bool sendsActiveSeq,
                            uint32_t activeSeq, bool confirmed);

// Que se pinta: nada, la identidad completa, o (placa antigua con el bebe en su
// lista) el nombre en gris con "?" y sin edad ni minutos.
enum PatientDisplay {
  PATIENT_DISPLAY_HIDDEN = 0,
  PATIENT_DISPLAY_CONFIRMED,
  PATIENT_DISPLAY_UNCONFIRMED,
};
PatientDisplay patient_display_state(bool confirmed, bool listedOnLegacyBoard);

// "Hay un bebe bajo terapia AHORA y se sabe quien es": seq conocido, alguna
// terapia en marcha y la identidad confirmada. Es la definicion de
// BabyWizard_HasLiveSession(): exigir la confirmacion cierra de golpe todas las
// rutas que actuan sobre ese seq hacia la placa (alta, reenvio de SELECT,
// atajo del asistente) con una identidad restaurada que podria ser de otro.
bool patient_live_session(uint32_t seq, bool anyControlActive, bool confirmed);

// El `sn` no se puede verificar si falta en el registro o en la placa (0 =
// sin numero de serie): entonces `activeSeq` por si solo no basta para
// confirmar y se exige ademas el contraste de nombre y semanas con la lista.
bool patient_confirm_requires_list(uint32_t identitySn, uint32_t boardSn);

// Peticion propia de PROFILE_LIST: toca pedirla si no se ha agotado el numero
// de intentos y, o no hay ninguna en vuelo, o la que habia caduco (`retryMs`
// sin respuesta; sin esto una respuesta perdida bloqueaba los reintentos para
// siempre). Aritmetica modular: aguanta la vuelta de millis().
bool patient_list_request_due(bool outstanding, uint32_t askedAtMs,
                              uint32_t nowMs, uint8_t attempts,
                              uint8_t maxAttempts, uint32_t retryMs);

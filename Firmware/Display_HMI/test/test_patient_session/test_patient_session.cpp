// Especificaciones de modules/patient/patient_session: edad, formato,
// acumulador de terapia y registros de NVS. Spec:
// openspec/changes/hmi-patient-identity/specs/hmi-patient-identity/spec.md
#include <string.h>
#include <unity.h>

#include "modules/patient/patient_session.h"

void setUp(void) {}
void tearDown(void) {}

static const uint32_t DAY = 86400u;
static const uint32_t T0 = 1760000000u;  // un epoch cualquiera valido

// ---------------------------------------------------------------- edad

void test_age_same_day_as_capture(void) {
  PatientAge a = patient_age_compute(34, true, 5, T0, T0 + 3600u);
  TEST_ASSERT_TRUE(a.hasLifeDays);
  TEST_ASSERT_EQUAL_UINT32(5, a.lifeDays);
  TEST_ASSERT_TRUE(a.hasCorrected);
  TEST_ASSERT_EQUAL_UINT32(34, a.corrWeeks);
  TEST_ASSERT_EQUAL_UINT8(5, a.corrDays);
}

void test_age_days_elapsed_since_capture(void) {
  // 3 dias y 2 horas: solo cuentan los dias enteros.
  PatientAge a = patient_age_compute(34, true, 5, T0, T0 + 3 * DAY + 7200u);
  TEST_ASSERT_EQUAL_UINT32(8, a.lifeDays);
  TEST_ASSERT_EQUAL_UINT32(35, a.corrWeeks);
  TEST_ASSERT_EQUAL_UINT8(1, a.corrDays);
}

void test_age_just_under_a_day_adds_nothing(void) {
  PatientAge a = patient_age_compute(30, true, 2, T0, T0 + DAY - 1u);
  TEST_ASSERT_EQUAL_UINT32(2, a.lifeDays);
}

void test_age_exactly_one_day_adds_one(void) {
  PatientAge a = patient_age_compute(30, true, 2, T0, T0 + DAY);
  TEST_ASSERT_EQUAL_UINT32(3, a.lifeDays);
}

void test_age_no_clock_at_capture_keeps_captured_value(void) {
  PatientAge a = patient_age_compute(34, true, 5, 0, T0 + 10 * DAY);
  TEST_ASSERT_EQUAL_UINT32(5, a.lifeDays);
}

void test_age_no_clock_now_keeps_captured_value(void) {
  PatientAge a = patient_age_compute(34, true, 5, T0, 0);
  TEST_ASSERT_EQUAL_UINT32(5, a.lifeDays);
}

void test_age_clock_going_backwards_adds_nothing(void) {
  PatientAge a = patient_age_compute(34, true, 5, T0, T0 - 10 * DAY);
  TEST_ASSERT_EQUAL_UINT32(5, a.lifeDays);
}

void test_age_unknown_gives_nothing(void) {
  PatientAge a = patient_age_compute(34, false, 5, T0, T0 + DAY);
  TEST_ASSERT_FALSE(a.hasLifeDays);
  TEST_ASSERT_FALSE(a.hasCorrected);
}

void test_age_without_gestation_has_days_but_no_corrected(void) {
  PatientAge a = patient_age_compute(0, true, 12, T0, T0);
  TEST_ASSERT_TRUE(a.hasLifeDays);
  TEST_ASSERT_EQUAL_UINT32(12, a.lifeDays);
  TEST_ASSERT_FALSE(a.hasCorrected);
}

void test_age_corrected_rolls_over_weeks(void) {
  // 34+0 con 7 dias de vida = 35+0; con 8 = 35+1.
  PatientAge a = patient_age_compute(34, true, 7, T0, T0);
  TEST_ASSERT_EQUAL_UINT32(35, a.corrWeeks);
  TEST_ASSERT_EQUAL_UINT8(0, a.corrDays);
  a = patient_age_compute(34, true, 8, T0, T0);
  TEST_ASSERT_EQUAL_UINT32(35, a.corrWeeks);
  TEST_ASSERT_EQUAL_UINT8(1, a.corrDays);
}

void test_age_day_zero(void) {
  PatientAge a = patient_age_compute(36, true, 0, T0, T0);
  TEST_ASSERT_TRUE(a.hasLifeDays);
  TEST_ASSERT_EQUAL_UINT32(0, a.lifeDays);
  TEST_ASSERT_EQUAL_UINT32(36, a.corrWeeks);
  TEST_ASSERT_EQUAL_UINT8(0, a.corrDays);
}

void test_age_max_values_do_not_overflow(void) {
  PatientAge a = patient_age_compute(255, true, 65535, 1, 0xFFFFFFFFu);
  TEST_ASSERT_TRUE(a.hasLifeDays);
  TEST_ASSERT_TRUE(a.lifeDays >= 65535u);
  TEST_ASSERT_TRUE(a.hasCorrected);
}

// ------------------------------------------------------------- formato

static void expect_fmt(uint32_t min, const char *want) {
  char buf[16];
  size_t n = patient_fmt_minutes(min, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING(want, buf);
  TEST_ASSERT_EQUAL_UINT32(strlen(want), n);
}

void test_fmt_hours_and_minutes(void) { expect_fmt(200, "3h 20m"); }
void test_fmt_minutes_only(void) { expect_fmt(45, "45m"); }
void test_fmt_exact_hours(void) { expect_fmt(180, "3h"); }
void test_fmt_zero(void) { expect_fmt(0, "0m"); }
void test_fmt_worst_case_width(void) { expect_fmt(12 * 60 + 20, "12h 20m"); }
void test_fmt_one_hour_one_minute(void) { expect_fmt(61, "1h 1m"); }

void test_fmt_truncates_safely(void) {
  char buf[4];
  patient_fmt_minutes(200, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_UINT8('\0', buf[3]);
  TEST_ASSERT_EQUAL_UINT32(3, strlen(buf));
  patient_fmt_minutes(200, buf, 0);  // no escribe ni revienta
}

// ---------------------------------------------------------- acumulador

static PatientAccum started(uint32_t seq, PatientMinutes seed = {0, 0, 0}) {
  PatientAccum a;
  patient_accum_init(&a);
  patient_accum_begin(&a, seq, &seed);
  return a;
}

// Avanza `secs` segundos en pasos de 1 s con el estado dado.
static void run(PatientAccum *a, uint32_t *now, uint32_t secs, uint8_t act,
                bool photo, bool link = true) {
  for (uint32_t i = 0; i < secs; i++) {
    *now += 1000u;
    patient_accum_tick(a, *now, act, photo, link);
  }
}

void test_accum_temp_and_humidity_counts_both(void) {
  PatientAccum a = started(7);
  uint32_t now = 1000;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMP_AND_HUMIDITY, false, true);
  run(&a, &now, 120, PATIENT_ACT_TEMP_AND_HUMIDITY, false);
  PatientMinutes m = patient_accum_minutes(&a);
  TEST_ASSERT_EQUAL_UINT32(2, m.thermo);
  TEST_ASSERT_EQUAL_UINT32(2, m.humidity);
  TEST_ASSERT_EQUAL_UINT32(0, m.photo);
}

void test_accum_temperature_only(void) {
  PatientAccum a = started(7);
  uint32_t now = 1000;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  run(&a, &now, 60, PATIENT_ACT_TEMPERATURE, false);
  PatientMinutes m = patient_accum_minutes(&a);
  TEST_ASSERT_EQUAL_UINT32(1, m.thermo);
  TEST_ASSERT_EQUAL_UINT32(0, m.humidity);
}

void test_accum_humidity_only(void) {
  PatientAccum a = started(7);
  uint32_t now = 1000;
  patient_accum_tick(&a, now, PATIENT_ACT_HUMIDITY, false, true);
  run(&a, &now, 60, PATIENT_ACT_HUMIDITY, false);
  PatientMinutes m = patient_accum_minutes(&a);
  TEST_ASSERT_EQUAL_UINT32(0, m.thermo);
  TEST_ASSERT_EQUAL_UINT32(1, m.humidity);
  TEST_ASSERT_EQUAL_UINT32(0, m.photo);
}

void test_accum_photo_without_control(void) {
  PatientAccum a = started(7);
  uint32_t now = 1000;
  patient_accum_tick(&a, now, PATIENT_ACT_NONE, true, true);
  run(&a, &now, 60, PATIENT_ACT_NONE, true);
  PatientMinutes m = patient_accum_minutes(&a);
  TEST_ASSERT_EQUAL_UINT32(1, m.photo);
  TEST_ASSERT_EQUAL_UINT32(0, m.thermo);
  TEST_ASSERT_EQUAL_UINT32(0, m.humidity);
}

void test_accum_idle_counts_nothing(void) {
  PatientAccum a = started(7);
  uint32_t now = 1000;
  patient_accum_tick(&a, now, PATIENT_ACT_NONE, false, true);
  run(&a, &now, 300, PATIENT_ACT_NONE, false);
  PatientMinutes m = patient_accum_minutes(&a);
  TEST_ASSERT_EQUAL_UINT32(0, m.photo + m.thermo + m.humidity);
}

void test_accum_seed_is_the_starting_point(void) {
  PatientAccum a = started(7, PatientMinutes{100, 200, 45});
  PatientMinutes m = patient_accum_minutes(&a);
  TEST_ASSERT_EQUAL_UINT32(100, m.photo);
  TEST_ASSERT_EQUAL_UINT32(200, m.thermo);
  TEST_ASSERT_EQUAL_UINT32(45, m.humidity);
  uint32_t now = 1000;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  run(&a, &now, 60, PATIENT_ACT_TEMPERATURE, false);
  TEST_ASSERT_EQUAL_UINT32(201, patient_accum_minutes(&a).thermo);
}

void test_accum_first_tick_credits_nothing(void) {
  PatientAccum a = started(7);
  // Un primer tick con un now enorme no puede acreditar el hueco desde 0.
  patient_accum_tick(&a, 3000000u, PATIENT_ACT_TEMPERATURE, false, true);
  TEST_ASSERT_EQUAL_UINT32(0, patient_accum_minutes(&a).thermo);
}

void test_accum_subsecond_remainder_is_kept(void) {
  PatientAccum a = started(7);
  uint32_t now = 0;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  // 600 pasos de 100 ms = 60 s exactos: un minuto.
  for (int i = 0; i < 600; i++) {
    now += 100;
    patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  }
  TEST_ASSERT_EQUAL_UINT32(1, patient_accum_minutes(&a).thermo);
}

void test_accum_caps_a_stalled_step(void) {
  PatientAccum a = started(7);
  uint32_t now = 1000;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  now += 60000u;  // la UI se quedo 60 s sin pasar
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  // 5 s como mucho: ni de lejos un minuto.
  TEST_ASSERT_EQUAL_UINT32(0, patient_accum_minutes(&a).thermo);
  // ...y los 5 s si se han acreditado: 55 s mas completan el minuto.
  run(&a, &now, 55, PATIENT_ACT_TEMPERATURE, false);
  TEST_ASSERT_EQUAL_UINT32(1, patient_accum_minutes(&a).thermo);
}

void test_accum_frozen_while_link_is_lost(void) {
  PatientAccum a = started(7);
  uint32_t now = 1000;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, true, true);
  run(&a, &now, 30, PATIENT_ACT_TEMPERATURE, true, /*link=*/false);
  PatientMinutes m = patient_accum_minutes(&a);
  TEST_ASSERT_EQUAL_UINT32(0, m.thermo + m.photo);
  // Y al volver el enlace no se acredita el hueco: solo cuenta lo nuevo.
  run(&a, &now, 60, PATIENT_ACT_TEMPERATURE, true, true);
  TEST_ASSERT_EQUAL_UINT32(1, patient_accum_minutes(&a).thermo);
}

void test_accum_no_baby_counts_nothing(void) {
  PatientAccum a;
  patient_accum_init(&a);
  uint32_t now = 1000;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMP_AND_HUMIDITY, true, true);
  run(&a, &now, 120, PATIENT_ACT_TEMP_AND_HUMIDITY, true);
  PatientMinutes m = patient_accum_minutes(&a);
  TEST_ASSERT_EQUAL_UINT32(0, m.photo + m.thermo + m.humidity);
}

void test_accum_begin_zero_clears(void) {
  PatientAccum a = started(7, PatientMinutes{10, 20, 30});
  patient_accum_begin(&a, 0, nullptr);
  PatientMinutes m = patient_accum_minutes(&a);
  TEST_ASSERT_EQUAL_UINT32(0, m.photo + m.thermo + m.humidity);
  TEST_ASSERT_EQUAL_UINT32(0, a.seq);
}

void test_tick_reports_change_only_when_a_minute_turns(void) {
  PatientAccum a = started(7);
  uint32_t now = 0;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  int changes = 0;
  for (int i = 0; i < 120; i++) {
    now += 1000;
    if (patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true))
      changes++;
  }
  TEST_ASSERT_EQUAL_INT(2, changes);
}

// ------------------------------------------------------- resincronizar

void test_resync_stopped_counter_takes_the_board_value(void) {
  PatientAccum a = started(7, PatientMinutes{100, 0, 0});
  PatientMinutes board = {130, 0, 0};
  TEST_ASSERT_TRUE(patient_accum_resync(&a, 7, &board));
  TEST_ASSERT_EQUAL_UINT32(130, patient_accum_minutes(&a).photo);
}

void test_resync_stopped_counter_can_go_down(void) {
  // La placa manda: si ella dice menos y el contador no corre, se acepta.
  PatientAccum a = started(7, PatientMinutes{100, 0, 0});
  PatientMinutes board = {90, 0, 0};
  patient_accum_resync(&a, 7, &board);
  TEST_ASSERT_EQUAL_UINT32(90, patient_accum_minutes(&a).photo);
}

void test_resync_running_counter_keeps_the_larger(void) {
  PatientAccum a = started(7, PatientMinutes{0, 125, 0});
  uint32_t now = 0;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  run(&a, &now, 1, PATIENT_ACT_TEMPERATURE, false);  // ahora corre
  PatientMinutes board = {0, 120, 0};
  patient_accum_resync(&a, 7, &board);
  TEST_ASSERT_EQUAL_UINT32(125, patient_accum_minutes(&a).thermo);
}

void test_resync_running_counter_jumps_up_to_the_board(void) {
  PatientAccum a = started(7, PatientMinutes{0, 100, 0});
  uint32_t now = 0;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  run(&a, &now, 1, PATIENT_ACT_TEMPERATURE, false);
  PatientMinutes board = {0, 140, 0};
  patient_accum_resync(&a, 7, &board);
  TEST_ASSERT_EQUAL_UINT32(140, patient_accum_minutes(&a).thermo);
}

void test_resync_is_per_counter(void) {
  // Corre solo termo: foto (parada) obedece a la placa, termo conserva el mayor.
  PatientAccum a = started(7, PatientMinutes{50, 125, 0});
  uint32_t now = 0;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  run(&a, &now, 1, PATIENT_ACT_TEMPERATURE, false);
  PatientMinutes board = {10, 120, 0};
  patient_accum_resync(&a, 7, &board);
  PatientMinutes m = patient_accum_minutes(&a);
  TEST_ASSERT_EQUAL_UINT32(10, m.photo);
  TEST_ASSERT_EQUAL_UINT32(125, m.thermo);
}

void test_resync_ignores_other_baby(void) {
  PatientAccum a = started(7, PatientMinutes{100, 100, 100});
  PatientMinutes board = {1, 2, 3};
  TEST_ASSERT_FALSE(patient_accum_resync(&a, 8, &board));
  TEST_ASSERT_EQUAL_UINT32(100, patient_accum_minutes(&a).photo);
}

void test_resync_without_baby_does_nothing(void) {
  PatientAccum a;
  patient_accum_init(&a);
  PatientMinutes board = {1, 2, 3};
  TEST_ASSERT_FALSE(patient_accum_resync(&a, 0, &board));
  TEST_ASSERT_FALSE(patient_accum_resync(&a, 7, &board));
  TEST_ASSERT_FALSE(patient_accum_resync(&a, 7, nullptr));
}

void test_resync_reports_no_change_when_equal(void) {
  PatientAccum a = started(7, PatientMinutes{10, 20, 30});
  PatientMinutes board = {10, 20, 30};
  TEST_ASSERT_FALSE(patient_accum_resync(&a, 7, &board));
}

void test_resync_huge_board_value_does_not_overflow(void) {
  PatientAccum a = started(7);
  PatientMinutes board = {0xFFFFFFFFu, 0, 0};
  patient_accum_resync(&a, 7, &board);
  // Satura en lugar de dar la vuelta a un valor pequeno.
  TEST_ASSERT_TRUE(patient_accum_minutes(&a).photo >= 1000000u);
}

// ------------------------------------------------------------ volcado

void test_flush_not_due_when_clean(void) {
  PatientAccum a = started(7);
  patient_accum_flush_done(&a, 0);
  TEST_ASSERT_FALSE(patient_accum_flush_due(&a, 10 * PATIENT_FLUSH_INTERVAL_MS));
}

void test_flush_due_after_interval_when_dirty(void) {
  PatientAccum a = started(7);
  patient_accum_flush_done(&a, 0);
  uint32_t now = 0;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  run(&a, &now, 5, PATIENT_ACT_TEMPERATURE, false);  // 5 s, aun sin minuto
  TEST_ASSERT_TRUE(a.dirty);
  TEST_ASSERT_FALSE(patient_accum_flush_due(&a, PATIENT_FLUSH_INTERVAL_MS - 1));
  TEST_ASSERT_TRUE(patient_accum_flush_due(&a, PATIENT_FLUSH_INTERVAL_MS));
  patient_accum_flush_done(&a, PATIENT_FLUSH_INTERVAL_MS);
  TEST_ASSERT_FALSE(patient_accum_flush_due(&a, 2 * PATIENT_FLUSH_INTERVAL_MS));
}

void test_flush_due_immediately_when_everything_stops(void) {
  PatientAccum a = started(7);
  patient_accum_flush_done(&a, 0);
  uint32_t now = 0;
  patient_accum_tick(&a, now, PATIENT_ACT_TEMPERATURE, false, true);
  run(&a, &now, 10, PATIENT_ACT_TEMPERATURE, false);
  TEST_ASSERT_FALSE(patient_accum_flush_due(&a, now));
  run(&a, &now, 1, PATIENT_ACT_NONE, false);  // se paro todo
  TEST_ASSERT_TRUE(patient_accum_flush_due(&a, now));
}

void test_begin_marks_dirty(void) {
  PatientAccum a = started(7, PatientMinutes{10, 0, 0});
  TEST_ASSERT_TRUE(a.dirty);
}

// ------------------------------------------------------- registros NVS

static PatientIdentity sample_identity(void) {
  PatientIdentity id;
  memset(&id, 0, sizeof(id));
  id.seq = 42;
  strcpy(id.name, "Lucia");
  id.gestWeeks = 34;
  id.ageKnown = true;
  id.ageDays = 5;
  id.captureEpoch = T0;
  return id;
}

void test_identity_roundtrip(void) {
  PatientIdentity in = sample_identity();
  uint8_t blob[PATIENT_ID_RECORD_SIZE];
  TEST_ASSERT_TRUE(patient_identity_encode(&in, blob, sizeof(blob)));
  PatientIdentity out;
  memset(&out, 0xAA, sizeof(out));
  TEST_ASSERT_TRUE(patient_identity_decode(blob, sizeof(blob), &out));
  TEST_ASSERT_EQUAL_UINT32(42, out.seq);
  TEST_ASSERT_EQUAL_STRING("Lucia", out.name);
  TEST_ASSERT_EQUAL_UINT8(34, out.gestWeeks);
  TEST_ASSERT_TRUE(out.ageKnown);
  TEST_ASSERT_EQUAL_UINT16(5, out.ageDays);
  TEST_ASSERT_EQUAL_UINT32(T0, out.captureEpoch);
}

void test_identity_roundtrip_age_unknown_and_longest_name(void) {
  PatientIdentity in = sample_identity();
  in.ageKnown = false;
  in.ageDays = 0;
  in.captureEpoch = 0;
  memset(in.name, 'x', PATIENT_NAME_LEN - 1);
  in.name[PATIENT_NAME_LEN - 1] = '\0';
  uint8_t blob[PATIENT_ID_RECORD_SIZE];
  TEST_ASSERT_TRUE(patient_identity_encode(&in, blob, sizeof(blob)));
  PatientIdentity out;
  TEST_ASSERT_TRUE(patient_identity_decode(blob, sizeof(blob), &out));
  TEST_ASSERT_FALSE(out.ageKnown);
  TEST_ASSERT_EQUAL_UINT32(PATIENT_NAME_LEN - 1, strlen(out.name));
}

void test_identity_encode_rejects_zero_seq_and_small_buffer(void) {
  PatientIdentity in = sample_identity();
  uint8_t blob[PATIENT_ID_RECORD_SIZE];
  in.seq = 0;
  TEST_ASSERT_FALSE(patient_identity_encode(&in, blob, sizeof(blob)));
  in.seq = 1;
  TEST_ASSERT_FALSE(patient_identity_encode(&in, blob, sizeof(blob) - 1));
  TEST_ASSERT_FALSE(patient_identity_encode(nullptr, blob, sizeof(blob)));
}

void test_identity_decode_rejects_bad_records(void) {
  PatientIdentity in = sample_identity();
  uint8_t good[PATIENT_ID_RECORD_SIZE];
  TEST_ASSERT_TRUE(patient_identity_encode(&in, good, sizeof(good)));
  PatientIdentity out;

  // tamano incorrecto
  TEST_ASSERT_FALSE(patient_identity_decode(good, sizeof(good) - 1, &out));
  TEST_ASSERT_FALSE(patient_identity_decode(good, 0, &out));
  TEST_ASSERT_FALSE(patient_identity_decode(nullptr, sizeof(good), &out));

  // version desconocida
  uint8_t bad[PATIENT_ID_RECORD_SIZE];
  memcpy(bad, good, sizeof(bad));
  bad[0] = (uint8_t)(PATIENT_RECORD_VERSION + 1);
  TEST_ASSERT_FALSE(patient_identity_decode(bad, sizeof(bad), &out));

  // seq 0
  memcpy(bad, good, sizeof(bad));
  bad[4] = bad[5] = bad[6] = bad[7] = 0;
  TEST_ASSERT_FALSE(patient_identity_decode(bad, sizeof(bad), &out));

  // nombre sin terminador: los 24 bytes finales, todos distintos de cero
  memcpy(bad, good, sizeof(bad));
  memset(bad + (PATIENT_ID_RECORD_SIZE - PATIENT_NAME_LEN), 'z',
         PATIENT_NAME_LEN);
  TEST_ASSERT_FALSE(patient_identity_decode(bad, sizeof(bad), &out));
}

void test_identity_decode_failure_leaves_output_untouched(void) {
  PatientIdentity out = sample_identity();
  uint8_t junk[PATIENT_ID_RECORD_SIZE];
  memset(junk, 0xFF, sizeof(junk));
  TEST_ASSERT_FALSE(patient_identity_decode(junk, sizeof(junk), &out));
  TEST_ASSERT_EQUAL_UINT32(42, out.seq);
  TEST_ASSERT_EQUAL_STRING("Lucia", out.name);
}

void test_therapy_roundtrip_and_restore(void) {
  PatientAccum a = started(42, PatientMinutes{180, 200, 45});
  uint8_t blob[PATIENT_THER_RECORD_SIZE];
  TEST_ASSERT_TRUE(patient_therapy_encode(&a, blob, sizeof(blob)));
  PatientTherapyRecord rec;
  TEST_ASSERT_TRUE(patient_therapy_decode(blob, sizeof(blob), &rec));
  TEST_ASSERT_EQUAL_UINT32(42, rec.seq);
  TEST_ASSERT_EQUAL_UINT32(180u * 60u, rec.photoSec);

  PatientAccum b;
  patient_accum_init(&b);
  TEST_ASSERT_TRUE(patient_accum_restore(&b, &rec, 42));
  PatientMinutes m = patient_accum_minutes(&b);
  TEST_ASSERT_EQUAL_UINT32(180, m.photo);
  TEST_ASSERT_EQUAL_UINT32(200, m.thermo);
  TEST_ASSERT_EQUAL_UINT32(45, m.humidity);
  TEST_ASSERT_EQUAL_UINT32(42, b.seq);
}

void test_therapy_restore_rejects_other_baby(void) {
  PatientAccum a = started(42, PatientMinutes{1, 2, 3});
  uint8_t blob[PATIENT_THER_RECORD_SIZE];
  patient_therapy_encode(&a, blob, sizeof(blob));
  PatientTherapyRecord rec;
  patient_therapy_decode(blob, sizeof(blob), &rec);
  PatientAccum b;
  patient_accum_init(&b);
  TEST_ASSERT_FALSE(patient_accum_restore(&b, &rec, 43));
  TEST_ASSERT_FALSE(patient_accum_restore(&b, &rec, 0));
  TEST_ASSERT_EQUAL_UINT32(0, b.seq);
}

void test_therapy_decode_rejects_bad_records(void) {
  PatientAccum a = started(42, PatientMinutes{1, 2, 3});
  uint8_t good[PATIENT_THER_RECORD_SIZE];
  patient_therapy_encode(&a, good, sizeof(good));
  PatientTherapyRecord rec;
  TEST_ASSERT_FALSE(patient_therapy_decode(good, sizeof(good) + 1, &rec));
  TEST_ASSERT_FALSE(patient_therapy_decode(good, sizeof(good) - 1, &rec));
  uint8_t bad[PATIENT_THER_RECORD_SIZE];
  memcpy(bad, good, sizeof(bad));
  bad[0] = (uint8_t)(PATIENT_RECORD_VERSION + 1);
  TEST_ASSERT_FALSE(patient_therapy_decode(bad, sizeof(bad), &rec));
  memcpy(bad, good, sizeof(bad));
  bad[4] = bad[5] = bad[6] = bad[7] = 0;
  TEST_ASSERT_FALSE(patient_therapy_decode(bad, sizeof(bad), &rec));
}

void test_therapy_encode_refuses_without_baby(void) {
  PatientAccum a;
  patient_accum_init(&a);
  uint8_t blob[PATIENT_THER_RECORD_SIZE];
  TEST_ASSERT_FALSE(patient_therapy_encode(&a, blob, sizeof(blob)));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_age_same_day_as_capture);
  RUN_TEST(test_age_days_elapsed_since_capture);
  RUN_TEST(test_age_just_under_a_day_adds_nothing);
  RUN_TEST(test_age_exactly_one_day_adds_one);
  RUN_TEST(test_age_no_clock_at_capture_keeps_captured_value);
  RUN_TEST(test_age_no_clock_now_keeps_captured_value);
  RUN_TEST(test_age_clock_going_backwards_adds_nothing);
  RUN_TEST(test_age_unknown_gives_nothing);
  RUN_TEST(test_age_without_gestation_has_days_but_no_corrected);
  RUN_TEST(test_age_corrected_rolls_over_weeks);
  RUN_TEST(test_age_day_zero);
  RUN_TEST(test_age_max_values_do_not_overflow);

  RUN_TEST(test_fmt_hours_and_minutes);
  RUN_TEST(test_fmt_minutes_only);
  RUN_TEST(test_fmt_exact_hours);
  RUN_TEST(test_fmt_zero);
  RUN_TEST(test_fmt_worst_case_width);
  RUN_TEST(test_fmt_one_hour_one_minute);
  RUN_TEST(test_fmt_truncates_safely);

  RUN_TEST(test_accum_temp_and_humidity_counts_both);
  RUN_TEST(test_accum_temperature_only);
  RUN_TEST(test_accum_humidity_only);
  RUN_TEST(test_accum_photo_without_control);
  RUN_TEST(test_accum_idle_counts_nothing);
  RUN_TEST(test_accum_seed_is_the_starting_point);
  RUN_TEST(test_accum_first_tick_credits_nothing);
  RUN_TEST(test_accum_subsecond_remainder_is_kept);
  RUN_TEST(test_accum_caps_a_stalled_step);
  RUN_TEST(test_accum_frozen_while_link_is_lost);
  RUN_TEST(test_accum_no_baby_counts_nothing);
  RUN_TEST(test_accum_begin_zero_clears);
  RUN_TEST(test_tick_reports_change_only_when_a_minute_turns);

  RUN_TEST(test_resync_stopped_counter_takes_the_board_value);
  RUN_TEST(test_resync_stopped_counter_can_go_down);
  RUN_TEST(test_resync_running_counter_keeps_the_larger);
  RUN_TEST(test_resync_running_counter_jumps_up_to_the_board);
  RUN_TEST(test_resync_is_per_counter);
  RUN_TEST(test_resync_ignores_other_baby);
  RUN_TEST(test_resync_without_baby_does_nothing);
  RUN_TEST(test_resync_reports_no_change_when_equal);
  RUN_TEST(test_resync_huge_board_value_does_not_overflow);

  RUN_TEST(test_flush_not_due_when_clean);
  RUN_TEST(test_flush_due_after_interval_when_dirty);
  RUN_TEST(test_flush_due_immediately_when_everything_stops);
  RUN_TEST(test_begin_marks_dirty);

  RUN_TEST(test_identity_roundtrip);
  RUN_TEST(test_identity_roundtrip_age_unknown_and_longest_name);
  RUN_TEST(test_identity_encode_rejects_zero_seq_and_small_buffer);
  RUN_TEST(test_identity_decode_rejects_bad_records);
  RUN_TEST(test_identity_decode_failure_leaves_output_untouched);
  RUN_TEST(test_therapy_roundtrip_and_restore);
  RUN_TEST(test_therapy_restore_rejects_other_baby);
  RUN_TEST(test_therapy_decode_rejects_bad_records);
  RUN_TEST(test_therapy_encode_refuses_without_baby);
  return UNITY_END();
}

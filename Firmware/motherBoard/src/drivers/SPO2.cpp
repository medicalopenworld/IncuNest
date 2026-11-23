#include "SPO2.h"
#include "PpgSnapshot.h"
#include "esp32-hal-gpio.h"
#include "config/telemetry_keys.h"
#include "modules/control/afe_watchdog.h"
#include "modules/debug/afe_diag.h"
#include "modules/util/vitals_out.h"
#include "main.h"

INCUNEST_AFE4490 afe;
TaskHandle_t g_spo2_task = nullptr;

// What a consumer reads when there is no measurement: the five clinical output
// quantities and the three display quantities at NaN (the "no value" of
// incunest_afe4490, its spec 2.1.1 and 2.1.2) and the probe as disconnected,
// so that isfinite() is the only test a consumer needs. The display states say
// "no probe", as the library does while the probe is not applied. diag_code is
// kept: it is a diagnostic, not a measurement.
static AFE4490Data noValueSample(uint32_t diag_code) {
  AFE4490Data d = {};
  d.spo2 = NAN;
  d.spo2_r = NAN;
  d.spo2_r_method2_reg_corr = NAN;
  d.spo2_hold_reason = SpO2HoldReason::HOLD_PROBE;
  d.pi = NAN;
  d.hr1 = NAN;
  d.hr2 = NAN;
  d.hr3 = NAN;
  d.diag_code = diag_code;
  d.probe_state = ProbeState::PROBE_DISCONNECTED;
  d.spo2_disp = NAN;
  d.hr_disp = NAN;
  d.pi_disp = NAN;
  d.spo2_disp_state = AFE4490DispState::DISP_NO_PROBE;
  d.hr_disp_state = AFE4490DispState::DISP_NO_PROBE;
  d.pi_disp_state = AFE4490DispState::DISP_NO_PROBE;
  return d;
}

// The latest sample. SPO2_Task writes it and CommTask, the telemetry tasks and
// the factory test read it, from other tasks and the other core, so it is only
// touched inside this lock and only handed out whole (spo2Snapshot()). A
// spinlock rather than a mutex: the section is a copy of about 100 bytes.
static portMUX_TYPE s_latestMux = portMUX_INITIALIZER_UNLOCKED;
static AFE4490Data s_latest = noValueSample(0);
static uint32_t s_latestSeq = 0; // one more for every sample published

AFE4490Data spo2Snapshot(uint32_t *seq) {
  AFE4490Data d;
  portENTER_CRITICAL(&s_latestMux);
  d = s_latest;
  if (seq != nullptr) {
    *seq = s_latestSeq;
  }
  portEXIT_CRITICAL(&s_latestMux);
  return d;
}

static void publishSample(const AFE4490Data &d) {
  portENTER_CRITICAL(&s_latestMux);
  s_latest = d;
  s_latestSeq++;
  portEXIT_CRITICAL(&s_latestMux);
}

// One HR of the library and its SQI, when there is a value.
static void addHrTelemetry(JsonObject &json, const char *key,
                           const char *sqi_key, float hr, float sqi) {
  const uint16_t bpm = vitals_hr_bpm(hr); // 0 = no value
  if (bpm == 0) {
    return;
  }
  json[key] = (int)bpm;
  json[sqi_key] = roundSignificantDigits(sqi, TELEMETRIES_DECIMALS);
}

// The rule is the library's data contract (its spec 2.1.1): a quantity is sent
// when it is finite. No test on the SQI: spo2_sqi is a ramp of the PI between
// 0.5 and 2 %, 0 with a valid SpO2 at PI <= 0.5 %, and gating on it kept every
// low-perfusion point out of ThingsBoard until 18.57.0. No test on the probe
// state either: with the probe not applied, or the AFE stalled, every quantity
// of the sample is NaN (spo2Snapshot()).
void spo2AddTelemetry(JsonObject &json) {
  const AFE4490Data s = spo2Snapshot();
  if (vitals_has_value(s.spo2)) {
    json[SPO2_KEY] = roundSignificantDigits(s.spo2, TELEMETRIES_DECIMALS);
    json[SPO2_SQI_KEY] =
        roundSignificantDigits(s.spo2_sqi, TELEMETRIES_DECIMALS);
  }
  if (vitals_has_value(s.pi)) {
    json[PI_KEY] = roundSignificantDigits(s.pi, TELEMETRIES_DECIMALS);
  }
  addHrTelemetry(json, HR1_KEY, HR1_SQI_KEY, s.hr1, s.hr1_sqi);
  addHrTelemetry(json, HR2_KEY, HR2_SQI_KEY, s.hr2, s.hr2_sqi);
  addHrTelemetry(json, HR3_KEY, HR3_SQI_KEY, s.hr3, s.hr3_sqi);
}

// PpgSnapshot stores every sample SPO2_Task reads, with no decimation.
static_assert(1000 / SPO2_TASK_PERIOD_MS == PPG_SNAPSHOT_SRC_HZ,
              "SPO2_Task's rate and PpgSnapshot's source rate must be the same");

#define SPO2_LOG_INTERVAL_SAMPLES 50 // ~1 s at 50 reads a second

// Detector de parada y caducidad del dato (logica pura en
// modules/control/afe_watchdog.*). Solo lo toca SPO2_Task tras crearse;
// initSPO2() lo arma antes de lanzarla. NO reinicia la libreria: ver el
// porque en afe_watchdog.h.
static AfeWatchdog s_afeWd;

#ifndef AFE_DIAG_DRAIN_ALL
#define AFE_DIAG_DRAIN_ALL 1
#endif

// DIAGNOSTIC BUILD (modules/debug/afe_diag.h): hands one sample to the
// detector of spikes. Observes only.
[[maybe_unused]] static void afeDiagFeed(const AFE4490Data &d, uint32_t now_ms) {
  AfeDiagSample s;
  s.led1 = d.led1;
  s.led2 = d.led2;
  s.aled1 = d.aled1;
  s.aled2 = d.aled2;
  s.ppg_disp = d.ppg_disp;
  s.diag_code = d.diag_code;
  s.applied = d.probe_state == ProbeState::PROBE_APPLIED;
  afe_diag_feed(&s, now_ms);
}

uint32_t afeDiagCode(void) { return spo2Snapshot().diag_code; }
uint32_t afeStallCount(void) { return afe_wd_stalls(&s_afeWd); }

// Reset fisico del AFE por PDN (IO7, activo bajo, en la V19). En V17/V18 el
// pin es FAKE_PIN (46): sin PDN conectado, el pulso es inocuo pero no resetea
// nada. Bloquea ~200 ms: SOLO se usa en el arranque, desde initSPO2().
static void afePdnPulse(void) {
  pinMode(AFE44XX_PWDN_PIN, OUTPUT);
  digitalWrite(AFE44XX_PWDN_PIN, LOW);
  vTaskDelay(pdMS_TO_TICKS(100));
  digitalWrite(AFE44XX_PWDN_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(100));
}

void SPO2_Task(void *pvParameters) {
  AFE4490Data data;
  static uint32_t sample_count = 0;
  bool no_value_published = false; // the stalled AFE has already been reported
  TickType_t last_wake = xTaskGetTickCount();

  for (;;) {
    // A2: a sample older than AFE_WD_DATA_STALE_MS is not current. If the AFE
    // stalls, the library delivers nothing, so the last sample (probe APPLIED,
    // the last HR and SpO2) would stay published and CommTask, GPRS and WiFi
    // would send it as live vitals. The whole sample is replaced by one with no
    // measurement (noValueSample()): until 18.57.0 only probe_state changed and
    // the values stayed behind it. The next real sample overwrites it.
    {
      const uint32_t now = millis();
      if (!afe_wd_data_fresh(&s_afeWd, now) && !no_value_published) {
        publishSample(noValueSample(spo2Snapshot().diag_code));
        no_value_published = true;
      }
      // Un solo evento por episodio de parada. logE (no ESP_LOGE directo) para
      // que la linea entre en el anillo del CrashReporter.
      if (afe_wd_stall_detected(&s_afeWd, now)) {
        char msg[96];
        snprintf(msg, sizeof(msg),
                 "[AFE] sin muestras > %u ms (parada #%lu): dato caducado, "
                 "probe_state=DISCONNECTED",
                 (unsigned)AFE_WD_STALL_MS,
                 (unsigned long)afe_wd_stalls(&s_afeWd));
        logE(String(msg));
      }
    }
    // ONE read per turn, 50 turns a second (SPO2_TASK_PERIOD_MS). The library
    // produces 500 samples a second into a queue of 10 that drops the oldest
    // when full, so each read takes the oldest of the last ten: one sample of
    // every ten, about 20 ms old. Nothing in this firmware needs the other
    // nine: the display values (spo2_disp, hr_disp, pi_disp) and the probe
    // state are computed by the library on every sample and read here as they
    // stand, and the waveform goes to the HMI and to PpgSnapshot at 50 Hz, with
    // the library's display band-pass (f_high 20 Hz, initSPO2()) as the
    // anti-aliasing filter. Until 18.58.0 this task read every sample (1 ms
    // period) and each consumer picked its own one of ten.
    if (afe.getData(data)) {
      afe_wd_sample(&s_afeWd, millis());
      publishSample(data);
      no_value_published = false;
      ppgSnapshotFeed(data, millis());

      // ESTA LINEA DE LOG TUMBO LA PLACA (banco 2026-09-14). El coredump:
      //
      //   #13 std::string::append(" HR3=")
      //   #6  operator new (sz=323)  -> lanza std::bad_alloc
      //   #4  std::terminate()  ->  abort()
      //
      // Antes se montaba encadenando ~20 concatenaciones de String: cada `+`
      // crea un temporal y realoca, y el resultado se pasaba POR VALOR, que es
      // una copia mas. Con el heap apretado una de esas realocaciones lanza, y
      // como aqui nadie captura bad_alloc, std::terminate llama a abort().
      //
      // Lo peor no es que asignara, es que asignaba PARA NADA:
      // LOG_PULSIOXIMETRY es false, asi que logSPO2() descarta la cadena nada
      // mas entrar. La placa que gobierna el calefactor se reiniciaba
      // construyendo una linea que no se imprime.
      //
      // Ahora: la guarda es de COMPILACION (LOG_PULSIOXIMETRY es un #define
      // false, asi que el bloque entero desaparece) y el formateo va a un
      // buffer de pila con snprintf. Cero asignaciones de heap en un camino
      // que corre a 50 Hz (500 Hz hasta la 18.58.0), encendido el log o apagado.
      if (++sample_count % SPO2_LOG_INTERVAL_SAMPLES == 0 && LOG_PULSIOXIMETRY) {
        char line[320];
        // ppg_disp is [-1, +1] since NORM_PI (lib v0.116+, selected in
        // initSPO2()); the 8 decimals date from the A/A era (v0.69..v0.115,
        // ~1e-6) and do no harm now.
        snprintf(line, sizeof(line),
                 "[SPO2] n=%lu PPG=%.8f RED=%d IR=%d RED_sub=%d IR_sub=%d "
                 "SpO2=%.1f SpO2_SQI=%.3f R=%.3f PI=%.2f "
                 "HR1=%.0f HR1_SQI=%.2f HR2=%.0f HR2_SQI=%.2f "
                 "HR3=%.0f HR3_SQI=%.2f DIAG=0x%lX",
                 (unsigned long)sample_count, (double)data.ppg_disp,
                 (int)data.led2, (int)data.led1, (int)data.led2_sub,
                 (int)data.led1_sub,
                 // -1 = no value (NaN). Never the SQI: a valid SpO2 at
                 // PI <= 0.5 % has spo2_sqi == 0.
                 (double)(vitals_has_value(data.spo2) ? data.spo2 : -1.0f),
                 (double)data.spo2_sqi, (double)data.spo2_r, (double)data.pi,
                 (double)(vitals_has_value(data.hr1) ? data.hr1 : -1.0f),
                 (double)data.hr1_sqi,
                 (double)(vitals_has_value(data.hr2) ? data.hr2 : -1.0f),
                 (double)data.hr2_sqi,
                 (double)(vitals_has_value(data.hr3) ? data.hr3 : -1.0f),
                 (double)data.hr3_sqi, (unsigned long)data.diag_code);
        logSPO2(line);
      }

      // DIAGNOSTIC BUILD (modules/debug/afe_diag.h). What was published above
      // is the same sample as without it: the oldest in the queue. The other
      // samples of this turn, which the normal build leaves in the queue for
      // the library to drop, are taken here and looked at one by one, so that
      // the detector sees all 500 a second. Nothing of them is published.
      // AFE_DIAG_DRAIN_ALL=0 is the control: one sample per turn and the rest
      // left in the queue, exactly as 18.60.0, with the detector unfed.
      {
        const uint32_t now = millis();
        int drained = 1;
#if AFE_DIAG_DRAIN_ALL
        afeDiagFeed(data, now);
        AFE4490Data more;
        while (afe.getData(more)) {
          afeDiagFeed(more, now);
          drained++;
        }
#endif
        afe_diag_turn(drained, now);
      }
    } else {
      afe_diag_turn(0, millis());
    }
    // A fixed period, not a delay after the work: CTRL,PPG goes out once per
    // sample read here, and a period that stretched by a tick now and then
    // would make the HMI sweep slow. After a late turn (more than a period
    // behind) start again from now instead of catching up with a burst of
    // reads, which would hand out consecutive samples 2 ms apart as if they
    // were 20 ms apart.
    const TickType_t period = pdMS_TO_TICKS(SPO2_TASK_PERIOD_MS);
    vTaskDelayUntil(&last_wake, period);
    if ((TickType_t)(xTaskGetTickCount() - last_wake) > period) {
      last_wake = xTaskGetTickCount();
    }
  }
}

void initSPO2() {
  // Hard reset via PWDN pin. Real en la V19 (IO7); en V16/V17/V18 el pin es
  // FAKE_PIN=46: no hay PDN conectado y el pulso no hace nada.
  afePdnPulse();

  // Initialize SPI bus for AFE4490 (CS=-1: managed per device via AFE44XX_CS)
  SPI.begin(AFE_SCK, AFE_MISO, AFE_MOSI, -1);

  // HGAC (RF-only; since lib v0.81 two EMAs per domain — a fast HIGH2 guard plus
  // slow HIGH1/LOW1 levelling, so it now raises RF as well as lowering it): still
  // ships disabled. Enabled here because both LED channels sit at the ADC positive
  // rail (~2^21) with no probe applied, and the resulting DC step on probe
  // application swamps the SpO2 AC estimators.
  afe.setHgacEnable(true);

  // PPG display waveform (lib v0.116+, spec §5.9): NORM_PI is the product mode.
  // ppg_disp leaves the library centred and scaled to [-1, +1] by its own
  // slew-limited envelope tracker, with the amplitude following the perfusion
  // index (full size at PI >= 2 %, half at PI <= 0.5 %, logarithmic between), so
  // a shrinking trace still tells the operator the perfusion is falling. 0 while
  // the probe is not applied or during the tracker warm-up. The gain lives here,
  // in the library, not in CommTask (which only maps [-1, +1] to the wire byte).
  // Tracker and PI thresholds stay at the library defaults.
  afe.setPPGDispMode(AFE4490PPGDispMode::NORM_PI);

  // Display band-pass: f_high set here, explicitly, because it belongs to the
  // consumer. SPO2_Task takes ppg_disp at 50 Hz (Nyquist 25 Hz) for the HMI and
  // for PpgSnapshot (until 18.58.0 it was CommTask that sampled it at 50 Hz).
  // Since incunest_afe4490 v0.119 the upper side is a 5th-order Butterworth
  // (band-pass pole + two low-pass biquads) with f_high the -3 dB point of the
  // whole chain; the consumer rule of its spec 5.9 is f_high <= 0.4 x rate for
  // >= 10 dB at the consumer's Nyquist (and >= 40 dB at 2.5 x f_high),
  // f_high <= 0.3 x rate for >= 20 dB. With 20 Hz at 50 Hz: -10 dB at 25 Hz,
  // -41 dB at mains. Measured on SUBJ08-11 the content above 25 Hz is
  // negligible with either, and 20 Hz keeps the trace the library ships by
  // default (cost vs the 2nd-order chain: < 2 px on the flanks, < 0.3 px at
  // the systolic peak, +18 ms); 16 Hz would buy 20 dB at Nyquist for ~3 px.
  afe.setPPGDispFilter(0.5f, 20.0f);

  // Configure chip registers, attach DRDY ISR, launch internal processing task
  afe.begin(AFE44XX_CS, AFE_ADC_READY);

  // Antes de lanzar SPO2_Task, que es quien lo consulta.
  afe_wd_init(&s_afeWd, millis());
  afe_diag_init();  // DIAGNOSTIC BUILD

  // Launch application consumer task
  logSPO2("Creating SPO2 task ...\n");
  while (xTaskCreatePinnedToCore(SPO2_Task, "SPO2", SMALL_TASK_STACK_BYTES, NULL,
                                 SPO2_TASK_PRIORITY, &g_spo2_task,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logSPO2("SPO2 task successfully created!\n");
}

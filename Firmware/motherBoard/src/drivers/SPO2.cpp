#include "SPO2.h"
#include "DriveUpload.h"
#include "PpgSnapshot.h"
#include "esp32-hal-gpio.h"
#include "modules/control/afe_watchdog.h"
#include "main.h"

INCUNEST_AFE4490 afe;
TaskHandle_t g_spo2_task = nullptr;
volatile AFE4490Data g_spo2_data = {};

#define SPO2_LOG_INTERVAL_SAMPLES 500 // ~1 s at 500 Hz

// Detector de parada y caducidad del dato (logica pura en
// modules/control/afe_watchdog.*). Solo lo toca SPO2_Task tras crearse;
// initSPO2() lo arma antes de lanzarla. NO reinicia la libreria: ver el
// porque en afe_watchdog.h.
static AfeWatchdog s_afeWd;

uint32_t afeDiagCode(void) { return g_spo2_data.diag_code; }
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

  for (;;) {
    // A2: un dato de mas de AFE_WD_DATA_STALE_MS no es actual. Si el AFE se
    // para, g_spo2_data se queda con el ultimo probe_state (APPLIED) y las
    // ultimas FC/SpO2, y CommTask, GPRS y WiFi las publicarian congeladas como
    // si fueran vitales en vivo. Se sintetiza PROBE_DISCONNECTED ("sin senal"):
    // todos los consumidores ya tratan "no APPLIED" como "sin vitales". La
    // siguiente muestra real lo sobrescribe (memcpy de abajo).
    {
      const uint32_t now = millis();
      if (!afe_wd_data_fresh(&s_afeWd, now) &&
          g_spo2_data.probe_state != ProbeState::PROBE_DISCONNECTED) {
        g_spo2_data.probe_state = ProbeState::PROBE_DISCONNECTED;
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
    if (afe.getData(data)) {
      afe_wd_sample(&s_afeWd, millis());
      memcpy((void *)&g_spo2_data, &data, sizeof(data));
      if (data.probe_state == ProbeState::PROBE_APPLIED)
        drivePushSample(data);
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
      // que corre a 500 Hz, encendido el log o apagado.
      if (++sample_count % SPO2_LOG_INTERVAL_SAMPLES == 0 && LOG_PULSIOXIMETRY) {
        char line[320];
        // ppg_disp es float (A/A) desde la v0.69 de la libreria; con %.2f
        // saldria 0.00 en cada muestra, de ahi los 8 decimales.
        snprintf(line, sizeof(line),
                 "[SPO2] n=%lu PPG=%.8f RED=%d IR=%d RED_sub=%d IR_sub=%d "
                 "SpO2=%.1f SpO2_SQI=%.3f R=%.3f PI=%.2f "
                 "HR1=%.0f HR1_SQI=%.2f HR2=%.0f HR2_SQI=%.2f "
                 "HR3=%.0f HR3_SQI=%.2f DIAG=0x%lX",
                 (unsigned long)sample_count, (double)data.ppg_disp,
                 (int)data.led2, (int)data.led1, (int)data.led2_sub,
                 (int)data.led1_sub,
                 (double)(data.spo2_sqi > 0.0f ? data.spo2 : -1.0f),
                 (double)data.spo2_sqi, (double)data.spo2_r, (double)data.pi,
                 (double)(data.hr1_sqi > 0.0f ? data.hr1 : -1.0f),
                 (double)data.hr1_sqi,
                 (double)(data.hr2_sqi > 0.0f ? data.hr2 : -1.0f),
                 (double)data.hr2_sqi,
                 (double)(data.hr3_sqi > 0.0f ? data.hr3 : -1.0f),
                 (double)data.hr3_sqi, (unsigned long)data.diag_code);
        logSPO2(line);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(SPO2_TASK_PERIOD_MS));
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

  // Configure chip registers, attach DRDY ISR, launch internal processing task
  afe.begin(AFE44XX_CS, AFE_ADC_READY);

  // Antes de lanzar SPO2_Task, que es quien lo consulta.
  afe_wd_init(&s_afeWd, millis());

  // Launch application consumer task
  logSPO2("Creating SPO2 task ...\n");
  while (xTaskCreatePinnedToCore(SPO2_Task, "SPO2", SMALL_TASK_STACK_BYTES, NULL,
                                 SPO2_TASK_PRIORITY, &g_spo2_task,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logSPO2("SPO2 task successfully created!\n");
}

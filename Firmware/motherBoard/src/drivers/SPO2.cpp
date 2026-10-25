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

// Vigilante de muestras (logica pura en modules/control/afe_watchdog.*). Solo
// lo toca SPO2_Task tras crearse; initSPO2() lo arma antes de lanzarla.
static AfeWatchdog s_afeWd;

uint32_t afeDiagCode(void) { return g_spo2_data.diag_code; }
uint32_t afeResetCount(void) { return afe_wd_restarts(&s_afeWd); }

// Reset fisico del AFE por PDN (IO7, activo bajo, en la V19). En V17/V18 el
// pin es FAKE_PIN (46): sin PDN conectado, el pulso es inocuo pero no resetea
// nada. Bloquea ~200 ms: llamar solo desde una tarea.
static void afePdnPulse(void) {
  pinMode(AFE44XX_PWDN_PIN, OUTPUT);
  digitalWrite(AFE44XX_PWDN_PIN, LOW);
  vTaskDelay(pdMS_TO_TICKS(100));
  digitalWrite(AFE44XX_PWDN_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(100));
}

// Configura y arranca la libreria del AFE. Comun al arranque y al reinicio del
// vigilante. HGAC (RF-only; since lib v0.81 two EMAs per domain — a fast HIGH2
// guard plus slow HIGH1/LOW1 levelling, so it now raises RF as well as
// lowering it): still ships disabled. Enabled here because both LED channels
// sit at the ADC positive rail (~2^21) with no probe applied, and the
// resulting DC step on probe application swamps the SpO2 AC estimators.
static void afeConfigureAndBegin(void) {
  afe.setHgacEnable(true);
  // Configure chip registers, attach DRDY ISR, launch internal processing task
  afe.begin(AFE44XX_CS, AFE_ADC_READY);
}

#if AFE_WATCHDOG_ENABLED
// Reinicio completo del AFE: la libreria no tiene end(); su equivalente es
// stop() (desengancha la ISR de DRDY, espera a _spi_mutex, borra sus 4 tareas y
// objetos FreeRTOS) y begin() lo vuelve a crear todo. Entre medias, un reset
// fisico por PDN. SPI.begin() no se repite.
//
// RIESGO CONOCIDO: stop() toma _spi_mutex con portMAX_DELAY. Si la tarea
// interna murio con el mutex cogido, se queda aqui para siempre. Solo bloquea
// esta tarea (el loopTask sigue alimentando el Task WDT) y no empeora un AFE
// que ya estaba colgado. Ver design.md D5 de mb-hw19.
static void afeRestart(void) {
  ESP_LOGE("SPO2",
           "[AFE] sin muestras > %u ms: reinicio #%lu (stop, PDN, begin)",
           (unsigned)AFE_WD_STALL_MS, (unsigned long)afe_wd_restarts(&s_afeWd));
  afe.stop();
  afePdnPulse();
  afeConfigureAndBegin();
}
#endif

void SPO2_Task(void *pvParameters) {
  AFE4490Data data;
  static uint32_t sample_count = 0;

  for (;;) {
#if AFE_WATCHDOG_ENABLED
    if (afe_wd_should_restart(&s_afeWd, millis())) {
      afeRestart();
    }
#endif
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

  afeConfigureAndBegin();

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

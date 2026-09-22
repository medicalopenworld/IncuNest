#include "SPO2.h"
#include "DriveUpload.h"
#include "PpgSnapshot.h"
#include "esp32-hal-gpio.h"
#include "main.h"

INCUNEST_AFE4490 afe;
TaskHandle_t g_spo2_task = nullptr;
volatile AFE4490Data g_spo2_data = {};

#define SPO2_LOG_INTERVAL_SAMPLES 500 // ~1 s at 500 Hz

void SPO2_Task(void *pvParameters) {
  AFE4490Data data;
  static uint32_t sample_count = 0;

  for (;;) {
    if (afe.getData(data)) {
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
                 "HR3=%.0f HR3_SQI=%.2f",
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
                 (double)data.hr3_sqi);
        logSPO2(line);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(SPO2_TASK_PERIOD_MS));
  }
}

void initSPO2() {
  // Hard reset via PWDN pin (FAKE_PIN=46 on V16: no physical PWDN connected)
  pinMode(AFE44XX_PWDN_PIN, OUTPUT);
  digitalWrite(AFE44XX_PWDN_PIN, LOW);
  vTaskDelay(pdMS_TO_TICKS(100));
  digitalWrite(AFE44XX_PWDN_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(100));

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

  // Launch application consumer task
  logSPO2("Creating SPO2 task ...\n");
  while (xTaskCreatePinnedToCore(SPO2_Task, "SPO2", 4096, NULL,
                                 SPO2_TASK_PRIORITY, &g_spo2_task,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logSPO2("SPO2 task successfully created!\n");
}

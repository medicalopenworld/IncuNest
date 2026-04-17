#include "SPO2.h"
#include "main.h"

INCUNEST_AFE4490    afe;
TaskHandle_t        g_spo2_task = nullptr;
volatile AFE4490Data g_spo2_data = {};

#define SPO2_LOG_INTERVAL_SAMPLES 500 // ~1 s at 500 Hz

void SPO2_Task(void *pvParameters) {
  AFE4490Data data;
  static uint32_t sample_count = 0;

  for (;;) {
    if (afe.getData(data)) {
      memcpy((void*)&g_spo2_data, &data, sizeof(data));

      if (++sample_count % SPO2_LOG_INTERVAL_SAMPLES == 0) {
        logSPO2("[SPO2] n=" + String(sample_count) +
                " PPG=" + String(data.ppg, 4) + " RED=" + String(data.led2) +
                " IR=" + String(data.led1) + " RED_sub=" +
                String(data.led2_aled2) + " IR_sub=" + String(data.led1_aled1) +
                " SpO2=" + String(data.spo2_sqi > 0.0f ? data.spo2 : -1.0f, 1) +
                " SpO2_SQI=" + String(data.spo2_sqi, 3) +
                " R=" + String(data.spo2_r, 3) + " PI=" + String(data.pi, 2) +
                " HR1=" + String(data.hr1_sqi > 0.0f ? data.hr1 : -1.0f, 0) +
                " HR1_SQI=" + String(data.hr1_sqi, 2) +
                " HR2=" + String(data.hr2_sqi > 0.0f ? data.hr2 : -1.0f, 0) +
                " HR2_SQI=" + String(data.hr2_sqi, 2) +
                " HR3=" + String(data.hr3_sqi > 0.0f ? data.hr3 : -1.0f, 0) +
                " HR3_SQI=" + String(data.hr3_sqi, 2));
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

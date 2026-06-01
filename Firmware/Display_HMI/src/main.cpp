#include "main.h"
// #include "AudioManager.h"  // Deshabilitado para migración Arduino 3.x
#include "CommTask.h"
#include "state/hmi_state.h"
#include "UITask.h"
#include "Wifi_OTA.h"
#include "esp_log.h"
#include <PCA9557.h>
#include <Preferences.h>
#include <lvgl.h>

static const char *TAG = "Main";

bool OTA_inprogress = false;
in3ator_parameters in3;

uint32_t g_hmiBootCount   = 0;
int      g_hmiLastRst     = 0;
bool     g_hmiRestoreState = false;

#ifdef CRASH_TEST
// Build-flag gated test harness. Compile with e.g. `-DCRASH_TEST=1` to fire a
// panic N seconds after boot so the motherboard can validate its crash
// capture pipeline. Remove the flag for production builds.
//   CRASH_TEST=1 -> null-pointer LoadProhibited
//   CRASH_TEST=2 -> abort()
//   CRASH_TEST=3 -> interrupt watchdog (disable IRQs + spin)
#ifndef CRASH_TEST_DELAY_S
#define CRASH_TEST_DELAY_S 30
#endif
static void CrashTestTask(void *pv) {
  for (int i = CRASH_TEST_DELAY_S; i > 0; i--) {
    ESP_LOGW(TAG, "CRASH_TEST=%d firing in %d s", CRASH_TEST, i);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  ESP_LOGE(TAG, "CRASH_TEST=%d: crashing now", CRASH_TEST);
  vTaskDelay(pdMS_TO_TICKS(50)); // let UART drain
#if CRASH_TEST == 1
  {
    volatile int *p = (int *)0;
    *p = 0xDEAD;
  }
#elif CRASH_TEST == 2
  abort();
#elif CRASH_TEST == 3
  taskDISABLE_INTERRUPTS();
  while (1) {
  }
#else
#error "CRASH_TEST must be 1, 2, or 3"
#endif
  vTaskDelete(NULL);
}
#endif

#ifdef CRASH_TEST_HMI
// HMI crash-simulation harness — mirrors CRASH_TEST_MB on the motherboard.
// Compile with -DCRASH_TEST_HMI=1 (null-ptr) or -DCRASH_TEST_HMI=2 (abort).
// Override delay with -DCRASH_TEST_HMI_DELAY_S=<n> (default 30s).
#ifndef CRASH_TEST_HMI_DELAY_S
#define CRASH_TEST_HMI_DELAY_S 30
#endif
static void CrashTestHMITask(void *pv) {
  for (int i = CRASH_TEST_HMI_DELAY_S; i > 0; i--) {
    ESP_LOGW(TAG, "[CRASH_TEST_HMI] HMI crash firing in %d s", i);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  ESP_LOGE(TAG, "[CRASH_TEST_HMI] Crashing now (CRASH_TEST_HMI=%d)", CRASH_TEST_HMI);
  vTaskDelay(pdMS_TO_TICKS(50));
#if CRASH_TEST_HMI == 1
  { volatile int *p = (int *)0; *p = 0xDEAD; }
#elif CRASH_TEST_HMI == 2
  abort();
#else
#error "CRASH_TEST_HMI must be 1 or 2"
#endif
  vTaskDelete(NULL);
}
#endif

void setup() {
  hmi_state_init();
  Serial.begin(SERIAL_BAUD);

  // Suppress ESP-IDF gpio error logs (caused by GT911 using pin -1)
  esp_log_level_set("gpio", ESP_LOG_NONE);

  {
    Preferences p;
    p.begin("diag", false);
    g_hmiBootCount = p.getUInt("boots", 0) + 1;
    p.putUInt("boots", g_hmiBootCount);
    esp_reset_reason_t rst = esp_reset_reason();
    g_hmiLastRst = (int)rst;
    p.putInt("last_rst", g_hmiLastRst);
    p.end();
    g_hmiRestoreState = (rst != ESP_RST_POWERON && rst != ESP_RST_BROWNOUT);
    ESP_LOGW(TAG, "[DIAG] HMI bootCount=%u lastRst=%d restoreState=%d",
             (unsigned)g_hmiBootCount, g_hmiLastRst, (int)g_hmiRestoreState);
  }

  ESP_LOGW(TAG, "BOOT heap: internal=%u SPIRAM=%u psramFound=%d psramSize=%u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (int)psramFound(), (unsigned)ESP.getPsramSize());

  initEEPROM();

  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);

  // AudioManager::getInstance().begin();

  /* Comentado para v1.3 - Control vía I2C @ 0x30
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);
  */

  // Power stability delay — only needed on cold power-on
  if (!g_hmiRestoreState) {
    delay(STARTUP_DELAY_MS);
  }

  LVGL_Mutex_Init();

  ESP_LOGI(TAG, "Creating OTA task ...");
  CreateOTATask();
  ESP_LOGI(TAG, "OTA task successfully created!");

  ESP_LOGI(TAG, "Creating Communication task ...");
  CreateCommTask();
  ESP_LOGI(TAG, "Communication task successfully created!");

  ESP_LOGI(TAG, "Creating UI task ...");
  CreateUITask();
  ESP_LOGI(TAG, "UI task successfully created!");

#ifdef CRASH_TEST
  xTaskCreatePinnedToCore(CrashTestTask, "CRASH_TEST", 2048, NULL, 1, NULL, 1);
#endif
#ifdef CRASH_TEST_HMI
  xTaskCreatePinnedToCore(CrashTestHMITask, "CRASH_TEST_HMI", 2048, NULL, 1, NULL, 1);
#endif
}

void loop() { vTaskDelay(pdMS_TO_TICKS(MS_PER_SECOND)); }
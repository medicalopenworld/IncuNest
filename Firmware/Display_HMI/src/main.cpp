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

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask,
                                               char *pcTaskName) {
  (void)xTask;
  ESP_EARLY_LOGE("STACK", "OVERFLOW in task '%s' — restarting", pcTaskName);
  esp_restart();
}

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

  // El "mayor bloque DMA" es el numero que decide si el HMI arranca: el panel
  // RGB pide DOS bounce buffers de 38,4 KB contiguos en SRAM interna
  // DMA-capaz, y el total libre no dice nada de si caben. Aqui, antes de
  // crear ninguna tarea, esta en su maximo; para cuando UI_Task crea el panel
  // WiFi ya se ha llevado ~85 KB y lo ha partido. Ver la escalera de
  // reintentos en UITask.cpp.
  ESP_LOGW(TAG,
           "BOOT heap: internal=%u SPIRAM=%u psramFound=%d psramSize=%u  "
           "[SRAM DMA] libre=%u mayor_bloque=%u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (int)psramFound(), (unsigned)ESP.getPsramSize(),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                             MALLOC_CAP_DMA),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL |
                                                      MALLOC_CAP_DMA));

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

  // La UI va PRIMERO, y las demas esperan a que tenga el panel: los dos
  // bounce buffers del panel RGB son 38,4 KB contiguos cada uno en SRAM
  // interna DMA-capaz, y son lo unico del arranque que necesita bloques tan
  // grandes. Arrancando WiFi antes, el mayor bloque libre bajaba de ~164 KB a
  // ~86 KB (medido en banco) y los 76,8 KB entraban por 9 KB — cualquier
  // buffer estatico nuevo dejaba el equipo en bucle de reinicio con la
  // pantalla negra. Con este orden el margen es de ~87 KB.
  ESP_LOGI(TAG, "Creating UI task ...");
  CreateUITask();
  ESP_LOGI(TAG, "UI task successfully created!");

  // CommTask va aqui, antes de la barrera: no pide bloques grandes (sus
  // anillos son estaticos) y adelantarla evita perder los CTRL,* que la placa
  // emita mientras el panel se crea.
  ESP_LOGI(TAG, "Creating Communication task ...");
  CreateCommTask();
  ESP_LOGI(TAG, "Communication task successfully created!");

  // Espera acotada: crear la tarea no reserva nada, la reserva ocurre dentro
  // de UI_Task (que ademas espera al STC8 del backlight). Sin esta barrera el
  // orden de arriba no sirve de nada, porque setup() seguiria y WiFi
  // fragmentaria la SRAM antes de que el panel la pidiese. El tope existe
  // para que un panel que no arranca no deje al equipo sin comunicacion con
  // la placa: sin display se monitoriza peor, pero sin CommTask no se
  // monitoriza nada.
  {
    const uint32_t t0 = millis();
    while (!UI_IsLcdPanelReady() && (millis() - t0) < LCD_READY_TIMEOUT_MS) {
      delay(5);
    }
    if (!UI_IsLcdPanelReady())
      ESP_LOGE(TAG, "panel RGB sin listo tras %lu ms — se sigue arrancando",
               (unsigned long)LCD_READY_TIMEOUT_MS);
    else
      ESP_LOGI(TAG, "panel RGB listo en %lu ms", (unsigned long)(millis() - t0));
  }

#ifndef DISABLE_WIFI_TEST
  ESP_LOGI(TAG, "Creating OTA task ...");
  CreateOTATask();
  ESP_LOGI(TAG, "OTA task successfully created!");
#else
  ESP_LOGW(TAG, "[DISABLE_WIFI_TEST] WiFi/OTA task NOT created — bench test build");
#endif

#ifdef CRASH_TEST
  xTaskCreatePinnedToCore(CrashTestTask, "CRASH_TEST", 2048, NULL, 1, NULL, 1);
#endif
#ifdef CRASH_TEST_HMI
  xTaskCreatePinnedToCore(CrashTestHMITask, "CRASH_TEST_HMI", 2048, NULL, 1, NULL, 1);
#endif
}

void loop() { vTaskDelay(pdMS_TO_TICKS(MS_PER_SECOND)); }
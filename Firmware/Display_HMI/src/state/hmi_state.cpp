#include "hmi_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static HmiState          g_state = {};
static SemaphoreHandle_t g_mutex = NULL;

void hmi_state_init(void) {
  g_mutex = xSemaphoreCreateRecursiveMutex();
  memset(&g_state, 0, sizeof(g_state));
  g_state.language = 1; // ENGLISH default
}

HmiState hmi_state_get(void) {
  HmiState copy;
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  copy = g_state;
  xSemaphoreGiveRecursive(g_mutex);
  return copy;
}

void hmi_state_set(const HmiState *s) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  g_state = *s;
  xSemaphoreGiveRecursive(g_mutex);
}

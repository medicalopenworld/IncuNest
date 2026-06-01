#include "state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static DeviceState       g_state  = {};
static SemaphoreHandle_t g_mutex  = NULL;
static uint32_t          g_alarms = 0;

void state_init(void) {
  g_mutex = xSemaphoreCreateRecursiveMutex();
  memset(&g_state, 0, sizeof(g_state));
  g_state.heaterSafeMAXPWM       = 255;
  g_state.alarmsEnabled          = true;
  g_state.photoFirstRun          = true;
  g_state.phototherapy_intensity = 255;
}

DeviceState state_get(void) {
  DeviceState copy;
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  copy = g_state;
  xSemaphoreGiveRecursive(g_mutex);
  return copy;
}

void state_set(const DeviceState *s) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  g_state = *s;
  xSemaphoreGiveRecursive(g_mutex);
}

double state_get_skin_temp(void) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  double v = g_state.temperatureSkin;
  xSemaphoreGiveRecursive(g_mutex);
  return v;
}
double state_get_air_temp(void) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  double v = g_state.temperatureAir;
  xSemaphoreGiveRecursive(g_mutex);
  return v;
}
double state_get_humidity(void) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  double v = g_state.humidity;
  xSemaphoreGiveRecursive(g_mutex);
  return v;
}
int state_get_actuation(void) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  int v = g_state.actuation;
  xSemaphoreGiveRecursive(g_mutex);
  return v;
}
bool state_get_phototherapy(void) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  bool v = g_state.phototherapy;
  xSemaphoreGiveRecursive(g_mutex);
  return v;
}

void state_set_alarm(uint8_t alarm_id, bool active) {
  if (alarm_id >= 10) return;
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  if (active) g_alarms |=  (1u << alarm_id);
  else        g_alarms &= ~(1u << alarm_id);
  g_state.alarmToReport[alarm_id] = active;
  xSemaphoreGiveRecursive(g_mutex);
}

bool state_get_alarm(uint8_t alarm_id) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  bool v = alarm_id < 10 && (g_alarms & (1u << alarm_id)) != 0;
  xSemaphoreGiveRecursive(g_mutex);
  return v;
}
uint32_t state_get_alarm_bitmask(void) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  uint32_t v = g_alarms;
  xSemaphoreGiveRecursive(g_mutex);
  return v;
}
void state_set_commstatus(uint8_t status) {
  xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY);
  g_state.commStatus = status;
  xSemaphoreGiveRecursive(g_mutex);
}

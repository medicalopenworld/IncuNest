#include "alarm_test.h"

namespace {

// La secuencia son cinco tramos: una rafaga de cada prioridad, de menor a
// mayor, separadas por silencio. De menor a mayor y no al reves para que el
// operador oiga la ESCALADA — es lo que permite juzgar de un vistazo que las
// tres suenan distinto, que es justamente lo que la prueba comprueba.
//
// Cada tramo con audio dura mas que su rafaga (ver los static_assert de
// Buzzer.cpp, que es el unico sitio que ve a la vez estas duraciones y las del
// patron). El sobrante es silencio: el motor de audio, al terminar la rafaga,
// se queda esperando su intervalo entre rafagas, que aqui nunca llega porque
// el tramo se acaba antes.
struct Phase {
  uint8_t priority;  // ALARM_TEST_IDLE en los huecos
  uint32_t ms;
};

const Phase kPhases[] = {
    {(uint8_t)ALARM_PRIORITY_LOW, ALARM_TEST_PHASE_MS_LOW},
    {ALARM_TEST_IDLE, ALARM_TEST_GAP_MS},
    {(uint8_t)ALARM_PRIORITY_MEDIUM, ALARM_TEST_PHASE_MS_MEDIUM},
    {ALARM_TEST_IDLE, ALARM_TEST_GAP_MS},
    {(uint8_t)ALARM_PRIORITY_HIGH, ALARM_TEST_PHASE_MS_HIGH},
};
const int kPhaseCount = (int)(sizeof(kPhases) / sizeof(kPhases[0]));

bool g_active = false;
int g_phase = 0;
uint32_t g_phaseStart = 0;

}  // namespace

void alarm_test_init(void) {
  g_active = false;
  g_phase = 0;
  g_phaseStart = 0;
}

bool alarm_test_start(uint32_t now_ms) {
  if (g_active) {
    return false;
  }
  g_active = true;
  g_phase = 0;
  g_phaseStart = now_ms;
  return true;
}

void alarm_test_tick(uint32_t now_ms) {
  if (!g_active) {
    return;
  }
  // Resta con signo: sobrevive al desbordamiento de millis() a los 24,86 dias,
  // que es el fallo que ya se pago una vez en alarm_machine.
  while (g_active &&
         (int32_t)(now_ms - g_phaseStart) >= (int32_t)kPhases[g_phase].ms) {
    g_phaseStart += kPhases[g_phase].ms;
    if (++g_phase >= kPhaseCount) {
      alarm_test_abort();
      return;
    }
  }
}

void alarm_test_abort(void) {
  g_active = false;
  g_phase = 0;
}

bool alarm_test_active(void) { return g_active; }

bool alarm_test_audio_required(void) {
  return g_active && kPhases[g_phase].priority != ALARM_TEST_IDLE;
}

uint8_t alarm_test_priority(void) {
  return g_active ? kPhases[g_phase].priority : ALARM_TEST_IDLE;
}

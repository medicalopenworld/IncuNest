#include "modules/brightness/brightness_policy.h"

// STUB (fase roja del TDD): compila y devuelve valores incorrectos a
// proposito. La implementacion llega en el commit siguiente.

uint8_t Brightness_ClampPct(uint8_t pct) { return pct; }

uint8_t Brightness_Target(const BrightnessInputs *in) {
  (void)in;
  return 0;
}

uint8_t Brightness_PercentToByte(uint8_t pct, bool levelsSupported) {
  (void)pct;
  (void)levelsSupported;
  return 0;
}

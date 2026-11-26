#include "drivers/hum_en_policy.h"

HumEnBoot hum_en_on_boot(bool forced, bool has_key, uint8_t stored) {
  if (!forced) return {1, true};
  return {has_key ? (uint8_t)(stored != 0) : HUM_EN_DEFAULT, false};
}

HumEnBoot hum_en_forced_off_on_boot(bool has_key, uint8_t stored) {
  return {0, !(has_key && stored == 0)};
}

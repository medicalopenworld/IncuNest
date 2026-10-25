#include "modules/brightness/brightness_policy.h"

uint8_t Brightness_ClampPct(uint8_t pct) {
  if (pct < BRIGHTNESS_MIN_PCT)
    return BRIGHTNESS_MIN_PCT;
  if (pct > BRIGHTNESS_MAX_PCT)
    return BRIGHTNESS_MAX_PCT;
  return pct;
}

uint8_t Brightness_Target(const BrightnessInputs *in) {
  // Sin entrada no hay nada que decidir: ante la duda, pantalla plena.
  if (in == nullptr)
    return BRIGHTNESS_MAX_PCT;

  // 1. Alarma: gana a todo. Nunca atenuada con una alarma activa.
  if (in->alarmActive)
    return BRIGHTNESS_MAX_PCT;

  const uint8_t user = Brightness_ClampPct(in->userPct);

  // 2. Modo noche: solo con la pantalla de bloqueo y sin tocar un rato. Nunca
  //    mas brillante que lo que eligio el usuario.
  if (in->nightMode && in->locked && in->idleSec >= BRIGHTNESS_NIGHT_DELAY_S)
    return user < BRIGHTNESS_NIGHT_PCT ? user : (uint8_t)BRIGHTNESS_NIGHT_PCT;

  // 3. Resto: el nivel del usuario.
  return user;
}

uint8_t Brightness_PercentToByte(uint8_t pct, bool levelsSupported) {
  if (!levelsSupported)
    return BRIGHTNESS_BYTE_FULL;

  const uint32_t p = pct > BRIGHTNESS_MAX_PCT ? (uint32_t)BRIGHTNESS_MAX_PCT
                                              : (uint32_t)pct;
  // Ciclo de trabajo (0..10000) con curva cuadratica; el byte es inverso.
  const uint32_t duty = p * p;
  const uint32_t span = BRIGHTNESS_BYTE_OFF - BRIGHTNESS_BYTE_FULL;
  const uint32_t drop = (duty * span + 5000u) / 10000u;
  uint32_t b = (uint32_t)BRIGHTNESS_BYTE_OFF - drop;
  if (b < BRIGHTNESS_BYTE_FULL)
    b = BRIGHTNESS_BYTE_FULL;
  if (b > BRIGHTNESS_BYTE_DIMMEST)
    b = BRIGHTNESS_BYTE_DIMMEST;
  return (uint8_t)b;
}

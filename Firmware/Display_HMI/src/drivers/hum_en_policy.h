#pragma once
#include <stdint.h>

// Control de humedad habilitado en Ajustes (clave hmi_cfg/hum_en).
//
// Habilitado por defecto desde HMI 4.9.0. Los equipos que ya traian firmware
// lo tenian deshabilitado (defecto 0), asi que el primer arranque con este
// firmware lo fuerza a 1 UNA vez y deja la marca hmi_cfg/hum_en_frc. Despues
// manda Ajustes: si alguien lo apaga, se respeta en los siguientes arranques.
constexpr uint8_t HUM_EN_DEFAULT = 1;

struct HumEnBoot {
  uint8_t value;  // valor de hum_en para este arranque
  bool write;     // grabar value en hum_en y poner la marca
};

// forced: la marca hum_en_frc ya existe. has_key/stored: estado de hum_en.
HumEnBoot hum_en_on_boot(bool forced, bool has_key, uint8_t stored);

// Equipos SIN hardware de humedad: compilar con -DHMI_HUMIDITY_FORCED_OFF=1.
// Entonces el control de humedad queda deshabilitado en CADA arranque (no una
// vez): aunque alguien lo active en Ajustes, al reiniciar vuelve a 0, porque en
// esos equipos no hay nada que regular. Solo escribe si hum_en no es ya 0, para
// no gastar la NVS en cada arranque. No toca la marca hum_en_frc.
#ifndef HMI_HUMIDITY_FORCED_OFF
#define HMI_HUMIDITY_FORCED_OFF 0
#endif

HumEnBoot hum_en_forced_off_on_boot(bool has_key, uint8_t stored);

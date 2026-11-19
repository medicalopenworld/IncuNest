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

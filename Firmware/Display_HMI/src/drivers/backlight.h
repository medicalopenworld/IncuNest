#pragma once
// Retroiluminacion del HMI: STC8H1K28 en I2C 0x30 (el mismo chip que da el
// zumbador y el altavoz del display). UN byte por transaccion, sin registro:
// 0 = maximo, 245 = apagado, 246..249 son opcodes de audio (nunca brillo).
//
// Aqui solo vive la transaccion. QUE nivel toca lo decide
// modules/brightness/brightness_policy, que es logica pura y se prueba en
// host.
//
// ARQ-LOCK-001: estas funciones hacen I2C, no se llaman con LVGL_Lock tomado.
#include <stdbool.h>
#include <stdint.h>

// Escribe el nivel (se sanea a [20, 100] %). Hasta `attempts` intentos (>= 1)
// con 50 ms entre ellos: el arranque pide 5, el servicio periodico 1 para no
// bloquear la UI (si falla, el llamador reintenta en su siguiente pasada).
// Devuelve true solo si el chip ACKeo el byte.
bool Backlight_SetPercent(uint8_t pct, uint8_t attempts);

// Apaga la pantalla (byte 245), hasta 3 intentos. Solo para el apagado del
// equipo (pwroff): es la unica via a 245.
bool Backlight_Off(void);

// Ultimo byte ACKeado por el chip, o -1 si todavia ninguno.
int Backlight_LastByte(void);

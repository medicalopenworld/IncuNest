#pragma once
// Politica de brillo de la retroiluminacion del HMI.
//
// Logica pura, sin Arduino ni LVGL: dados el nivel que ha elegido el usuario,
// si el modo noche esta activo, si la pantalla de bloqueo esta cargada, los
// segundos sin tocar y si hay alguna alarma, devuelve el porcentaje que debe
// tener la pantalla. Aparte de eso, convierte porcentaje a byte del STC8H1K28.
//
// Es una decision y no una transaccion de bus (esa es drivers/backlight)
// porque equivocarse aqui tiene un coste asimetrico: una pantalla demasiado
// brillante molesta, pero una pantalla atenuada con una alarma activa es un
// fallo de seguridad. Por eso la alarma es la PRIMERA regla y gana a todo.
//
// Tabla de estados (la primera fila que aplique gana):
//   1. alarma activa                                   -> 100 %
//   2. noche && bloqueada && idle >= NIGHT_DELAY_S     -> min(usuario, NOCHE)
//   3. cualquier otro caso                             -> usuario
// El nivel de usuario se sanea siempre a [MIN, 100].
#include <stdbool.h>
#include <stdint.h>

#define BRIGHTNESS_MIN_PCT 20       // suelo del slider y de cualquier nivel
#define BRIGHTNESS_MAX_PCT 100
#define BRIGHTNESS_DEFAULT_PCT 100  // el comportamiento de siempre (byte 10)
#define BRIGHTNESS_NIGHT_PCT 20     // nivel del modo noche
#define BRIGHTNESS_NIGHT_DELAY_S 30 // sin tocar y bloqueada, para atenuar

// Bytes del STC8H1K28 (0x30). El byte es INVERSO: 0 = maximo, 245 = apagado.
// 246..249 son opcodes del zumbador y del altavoz: jamas son brillo. Copia de
// DISPLAY_BL_ON_VALUE/DISPLAY_BL_OFF_VALUE (display_config.h) para que este
// modulo compile en host; drivers/backlight.cpp comprueba con static_assert
// que no se desincronizan.
#define BRIGHTNESS_BYTE_FULL 10
#define BRIGHTNESS_BYTE_OFF 245
// Byte mas oscuro que se emite como brillo (nunca se alcanza el 245).
#define BRIGHTNESS_BYTE_DIMMEST 240

typedef struct {
  uint8_t userPct;   // nivel elegido en Ajustes (se sanea a [MIN, MAX])
  bool nightMode;    // switch de modo noche
  bool locked;       // la pantalla de bloqueo esta cargada
  uint32_t idleSec;  // segundos desde el ultimo toque
  bool alarmActive;  // alguna alarma activa o enlace con la placa perdido
} BrightnessInputs;

#ifdef __cplusplus
extern "C" {
#endif

// Limita un nivel a [BRIGHTNESS_MIN_PCT, BRIGHTNESS_MAX_PCT].
uint8_t Brightness_ClampPct(uint8_t pct);

// Porcentaje objetivo para esa entrada. Siempre en [MIN, MAX].
uint8_t Brightness_Target(const BrightnessInputs *in);

// Porcentaje a byte del controlador. Curva cuadratica (la percepcion de brillo
// no es lineal) sobre el ciclo de trabajo, acotada a
// [BRIGHTNESS_BYTE_FULL, BRIGHTNESS_BYTE_DIMMEST]. 100 % -> BYTE_FULL.
// Cualquier pct > 100 se trata como 100. Con levelsSupported a false SIEMPRE
// da BRIGHTNESS_BYTE_FULL (degradacion a "encendido" si el chip no acepta
// niveles intermedios: el apagado solo existe via Backlight_Off()).
uint8_t Brightness_PercentToByte(uint8_t pct, bool levelsSupported);

#ifdef __cplusplus
}
#endif

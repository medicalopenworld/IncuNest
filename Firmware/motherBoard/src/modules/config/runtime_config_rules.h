#pragma once
// Reglas puras de la configuracion remota (openspec: mb-remote-config).
//
// Esta es la mitad SIN hardware del modulo runtime_config: la lista blanca de
// los nueve parametros, la regla de cada uno (rechazar o recortar), la lectura
// estricta de texto y de JSON y el formato del eco hacia ThingsBoard. No incluye
// Arduino.h ni toca NVS/in3 -- eso vive en runtime_config.{h,cpp} -- para poder
// probarse en host ([env:native]).
//
// UNA SOLA REGLA PARA CUALQUIER ORIGEN. POST /config, la consola USB y los
// atributos compartidos de ThingsBoard pasan todos por runtime_config_validate().
// La tabla completa (regla, clave NVS, efecto) esta en
// openspec/changes/mb-remote-config/design.md; en corto:
//
//   fan_supply_pwm, fan_ctl_pwm   entero 1..255            (rechaza)
//   fan_pid_en                    exactamente 0 o 1        (rechaza)
//   heater_amps                   1.0..12.0 A              (rechaza)
//   air_tmax, skin_tmax           alarm_clamp_*_cutout()   (RECORTA)
//   gprs_act, gprs_photo, gprs_stby  entero 10..86400 s    (rechaza)
//
// Solo se recortan los dos cortes termicos, que es lo que ya hacia el POST y
// cuyo rango es una propiedad normativa (shared/alarm_policy.h). Todo lo demas
// se rechaza: un PWM o un periodo mal tecleado que se convierte en silencio en
// otro valor es peor que no cambiar nada.

#include <stddef.h>
#include <stdint.h>

#include <ArduinoJson.h>

typedef enum {
  RCFG_FAN_SUPPLY_PWM = 0,
  RCFG_FAN_CTL_PWM,
  RCFG_FAN_PID_EN,
  RCFG_HEATER_AMPS,
  RCFG_AIR_TMAX,
  RCFG_SKIN_TMAX,
  RCFG_GPRS_ACT,
  RCFG_GPRS_PHOTO,
  RCFG_GPRS_STBY,
  RCFG_COUNT
} RuntimeConfigId;

typedef enum {
  RCFG_OK = 0,
  RCFG_OK_CLAMPED,       // aceptado, pero el valor aplicado difiere del pedido
  RCFG_REJ_NOT_FINITE,   // NaN o infinito
  RCFG_REJ_RANGE,        // fuera de rango, en un parametro que no se recorta
  RCFG_REJ_NOT_INTEGER,  // parametro entero con parte decimal
  RCFG_REJ_NOT_BOOL      // fan_pid_en distinto de 0 y 1
} RuntimeConfigStatus;

// Limites de las reglas. Estan aqui, y no repartidos por el .cpp, para que
// cambiar uno sea una edicion deliberada de una constante.
#define RCFG_PWM_MIN 1
#define RCFG_PWM_MAX 255
#define RCFG_HEATER_AMPS_MIN 1.0f
#define RCFG_HEATER_AMPS_MAX 12.0f
#define RCFG_PERIOD_MIN_S 10
#define RCFG_PERIOD_MAX_S 86400

// Tamano maximo del JSON del eco (9 claves a su maximo) -- el buffer del
// llamador debe tener al menos esto.
#define RCFG_ECHO_JSON_MAX 224

// Mascara con el bit de cada parametro.
#define RCFG_MASK(id) ((uint16_t)(1U << (unsigned)(id)))
#define RCFG_MASK_ALL ((uint16_t)((1U << RCFG_COUNT) - 1U))

// Nombre canonico (el de POST /config y el del atributo de ThingsBoard).
// NULL si id esta fuera de rango.
const char *runtime_config_name(RuntimeConfigId id);

// Busca un nombre en la lista blanca, sin distinguir mayusculas (la consola
// USB usa AIR_TMAX; la web y ThingsBoard usan air_tmax). false si no esta.
bool runtime_config_lookup(const char *name, RuntimeConfigId *id);

// Aplica la regla del parametro. En RCFG_OK y RCFG_OK_CLAMPED escribe *out; en
// cualquier rechazo NO lo toca.
RuntimeConfigStatus runtime_config_validate(RuntimeConfigId id, float in,
                                            float *out);

static inline bool runtime_config_status_accepted(RuntimeConfigStatus s) {
  return s == RCFG_OK || s == RCFG_OK_CLAMPED;
}

// Frase corta para el log y para el cuerpo del 400.
const char *runtime_config_status_text(RuntimeConfigStatus s);

// Lectura ESTRICTA de un valor de texto: toda la cadena (salvo espacios en los
// bordes) tiene que ser un unico numero finito. "", "abc", "12x" y "1e999"
// fallan. No toca *out si falla.
bool runtime_config_parse_number(const char *text, float *out);

// Lectura de un valor JSON: solo numero o booleano (true = 1, false = 0).
// Cadenas, null, arrays y objetos se rechazan. No toca *out si falla.
bool runtime_config_json_number(JsonVariantConst v, float *out);

// Formatea el eco: un objeto JSON con SOLO los parametros de `mask`, con el
// valor de values[id]. fan_pid_en sale como true/false para que haga
// ida y vuelta con el atributo booleano que escribe un widget de ThingsBoard.
// Devuelve la longitud, o 0 si la mascara esta vacia o no cabe en `len`.
size_t runtime_config_format_echo(const float values[RCFG_COUNT],
                                  uint16_t mask, char *buf, size_t len);

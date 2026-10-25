#pragma once
// Reglas puras de la configuracion remota (openspec: mb-remote-config).
//
// Esta es la mitad SIN hardware del modulo runtime_config: la lista blanca de
// los nueve parametros, la regla de cada uno (rechazar o recortar), las reglas
// extra de un origen remoto, la lectura estricta de texto y de JSON y el
// formato de los mensajes hacia ThingsBoard. No incluye Arduino.h ni toca
// NVS/in3 -- eso vive en runtime_config.{h,cpp} -- para poder probarse en host
// ([env:native]).
//
// UNA REGLA BASE PARA CUALQUIER ORIGEN. POST /config, la consola USB y los
// atributos compartidos de ThingsBoard pasan por runtime_config_validate*():
//
//   fan_supply_pwm, fan_ctl_pwm   entero 1..255            (rechaza)
//   fan_pid_en                    exactamente 0 o 1        (rechaza)
//   heater_amps                   1.0..12.0 A              (rechaza)
//   air_tmax, skin_tmax           34..40 C                 (web RECORTA; usb y
//                                                           ThingsBoard RECHAZAN)
//   gprs_act, gprs_photo, gprs_stby  entero 10..86400 s    (rechaza)
//
// Encima de eso, un origen ThingsBoard lleva las reglas de
// runtime_config_validate_remote() y la lista blanca reducida de
// runtime_config_remote_allowed(). Ver runtime_config_policy.h para el porque.

#include <stddef.h>
#include <stdint.h>

#include <ArduinoJson.h>

#include "modules/config/runtime_config_policy.h"

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
  RCFG_REJ_NOT_BOOL,     // fan_pid_en distinto de 0 y 1
  RCFG_REJ_ABOVE_FACTORY,   // remoto: por encima del valor de fabrica
  RCFG_REJ_BELOW_SETPOINT,  // remoto: corte por debajo de la consigna activa
  RCFG_REJ_NO_TACHOMETER,   // remoto: fan_* en una unidad sin tacometro
  RCFG_REJ_HEATER_ACTING,   // remoto: fan_* con el calefactor actuando
  RCFG_REJ_NOT_ALLOWED      // remoto: parametro fuera de la lista remota
} RuntimeConfigStatus;

// Limites de las reglas. Estan aqui, y no repartidos por el .cpp, para que
// cambiar uno sea una edicion deliberada de una constante.
#define RCFG_PWM_MIN 1
#define RCFG_PWM_MAX 255
#define RCFG_HEATER_AMPS_MIN 1.0f
#define RCFG_HEATER_AMPS_MAX 12.0f
#define RCFG_PERIOD_MIN_S 10
#define RCFG_PERIOD_MAX_S 86400

// Tamano maximo del JSON del eco (9 claves con su cfg_src_<clave>, a su
// maximo): el buffer del llamador debe tener al menos esto.
#define RCFG_ECHO_JSON_MAX 704
// Tamano maximo de un evento cfg_change.
#define RCFG_AUDIT_JSON_MAX 192

// Mascara con el bit de cada parametro.
#define RCFG_MASK(id) ((uint16_t)(1U << (unsigned)(id)))
#define RCFG_MASK_ALL ((uint16_t)((1U << RCFG_COUNT) - 1U))

// Nombre canonico (el de POST /config y el del atributo de ThingsBoard).
// NULL si id esta fuera de rango.
const char *runtime_config_name(RuntimeConfigId id);

// Busca un nombre canonico EXACTO (minusculas; POST /config y ThingsBoard).
// false si no esta en la lista blanca.
bool runtime_config_lookup(const char *name, RuntimeConfigId *id);

// La consola USB usa los nombres en MAYUSCULAS exactas (AIR_TMAX): es lo que
// aceptaba antes y asi cada origen sigue con su ortografia. No acepta la forma
// en minusculas.
bool runtime_config_lookup_console(const char *name, RuntimeConfigId *id);

// true para los seis parametros de seguridad (todos menos gprs_*).
bool runtime_config_is_safety(RuntimeConfigId id);

// Lista blanca de un origen ThingsBoard. Con safety_enabled = false solo los
// tres periodos GPRS.
bool runtime_config_remote_allowed(RuntimeConfigId id, bool safety_enabled);

// Rellena `out` con los ids de la lista remota efectiva, en orden, y devuelve
// cuantos son.
size_t runtime_config_remote_keys(bool safety_enabled,
                                  RuntimeConfigId out[RCFG_COUNT]);

// Regla base con RECORTE de los cortes termicos (lo que hace POST /config).
// En RCFG_OK y RCFG_OK_CLAMPED escribe *out; en cualquier rechazo NO lo toca.
RuntimeConfigStatus runtime_config_validate(RuntimeConfigId id, float in,
                                            float *out);

// Igual, pero air_tmax y skin_tmax fuera de 34..40 C se RECHAZAN
// (RCFG_REJ_RANGE) en lugar de recortarse. Consola USB y ThingsBoard.
RuntimeConfigStatus runtime_config_validate_strict(RuntimeConfigId id, float in,
                                                   float *out);

// Contexto de las reglas remotas: lo rellena el llamador con el estado de la
// unidad. Un setpoint <= 0 significa "sin control en curso en ese modo".
typedef struct {
  float air_factory_max;    // AIR_THERMAL_CUTOUT_DEFAULT_C
  float skin_factory_max;   // SKIN_TEMPERATURE_SET_MAX acotado al techo normativo
  float heater_factory_max; // HEATER_MAX_POWER_AMPS del HW
  bool fan_has_tach;        // in3.fanHasSpeedFeedback
  bool heater_acting;       // el calefactor esta actuando
  float air_setpoint_c;     // consigna de aire activa con control en curso, o 0
  float skin_setpoint_c;    // consigna de piel activa con control en curso, o 0
} RuntimeConfigRemoteCtx;

// Regla de un origen ThingsBoard: la estricta mas techo de fabrica, corte por
// encima de la consigna, y condiciones del ventilador. NO mira la lista blanca
// (runtime_config_remote_allowed) ni el aplazamiento: eso lo decide el
// llamador, para poder probar estas reglas aunque RCFG_REMOTE_SAFETY_PARAMS
// este a 0.
RuntimeConfigStatus runtime_config_validate_remote(
    RuntimeConfigId id, float in, const RuntimeConfigRemoteCtx *ctx,
    float *out);

// Un cambio remoto de seguridad con terapia activa no se aplica: queda
// pendiente hasta standby. Los periodos GPRS nunca se aplazan por esto.
bool runtime_config_should_defer(RuntimeConfigId id, bool therapy_active);

// true si ya paso `min_ms` desde el ultimo cambio de esa clave. Tolera la
// vuelta del contador de 32 bits.
bool runtime_config_rate_ok(bool has_last, uint32_t last_ms, uint32_t now_ms,
                            uint32_t min_ms);

static inline bool runtime_config_status_accepted(RuntimeConfigStatus s) {
  return s == RCFG_OK || s == RCFG_OK_CLAMPED;
}

// Frase corta para el log y para el cuerpo del 400.
const char *runtime_config_status_text(RuntimeConfigStatus s);

// Lectura ESTRICTA de un valor de texto: toda la cadena (salvo espacios en los
// bordes) tiene que ser un unico numero finito. "", "abc", "12x", "0x10" y
// "1e999" fallan. No toca *out si falla.
bool runtime_config_parse_number(const char *text, float *out);

// Lectura de un valor JSON: un numero; y un booleano SOLO si allow_bool (true
// = 1, false = 0), que es el caso unico de fan_pid_en. Cadenas, null, arrays y
// objetos se rechazan. No toca *out si falla.
bool runtime_config_json_number(JsonVariantConst v, bool allow_bool,
                                float *out);

// Valores heredados de NVS fuera de rango (una version anterior los guardaba
// sin comprobar): devuelve v si vale, def si no. Sirve para que un valor
// guardado antes no deje el formulario web devolviendo 400 en cada guardado.
int runtime_config_sanitize_period(int v, int def);
float runtime_config_sanitize_heater_amps(float v, float def);

// Formatea el eco: un objeto JSON con SOLO los parametros de `mask`, con el
// valor de values[id]. fan_pid_en sale como true/false. Si src[id] no es NULL
// ni vacio se anade "cfg_src_<clave>":"<src>@<epoch>" (o solo "<src>" si el
// epoch es 0). src y epoch pueden ser NULL. Devuelve la longitud, o 0 si la
// mascara esta vacia o no cabe en `len`.
size_t runtime_config_format_echo(const float values[RCFG_COUNT],
                                  uint16_t mask,
                                  const char *const src[RCFG_COUNT],
                                  const uint32_t epoch[RCFG_COUNT], char *buf,
                                  size_t len);

// Evento de auditoria cfg_change:
//   {"cfg_change":{"key":"air_tmax","old":38.00,"new":40.00,
//                  "origin":"tb-gprs","epoch":1730000000}}
// Sin JSON dinamico. Devuelve la longitud, o 0 si no cabe.
size_t runtime_config_format_audit(RuntimeConfigId id, float old_v, float new_v,
                                   const char *origin, uint32_t epoch,
                                   char *buf, size_t len);

// Atributo de cliente que dice que un valor remoto se rechazo:
//   {"cfg_rejected":"air_tmax: not allowed remotely"}
// `name` y `why` son texto propio del firmware (sin comillas). 0 si no cabe.
size_t runtime_config_format_rejected(const char *name, const char *why,
                                      char *buf, size_t len);

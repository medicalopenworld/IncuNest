#include "sb_telemetry.h"

#include "config/telemetry_keys.h"
#include "sb_protocol.h"

// TELEMETRIES_DECIMALS vive en main.h, que no compila para host. Es el mismo
// 2 de siempre; si cambiara alli, este numero tiene que seguirlo.
#define SB_TELEMETRY_DECIMALS 2

// Misma expresion que roundSignificantDigits() de system/math.cpp, copiada a
// proposito y no llamada: math.cpp entra por <Arduino.h> y su declaracion vive
// en main.h, que arrastra ThingsBoard. Ninguno de los dos compila para host, y
// build_src_filter del entorno native es global -- declararla y dejar que cada
// test la definiera rompia el enlace de TODOS los demas tests nativos. Tres
// lineas duplicadas cuestan menos que eso.
static double round_decimals(double value, int decimals) {
  double exponent = 1.0;
  for (int i = 0; i < decimals; i++) exponent *= 10.0;
  return (int)(value * exponent + 0.5) / exponent;
}

void sb_build_telemetry(JsonObject &json, const SbSnapshot &s, uint32_t now,
                        uint8_t env_used) {
  json[SB_LINK_OK_KEY] = s.link_ok;
  // Con el enlace caido los ultimos valores son viejos: mejor ninguna clave
  // que una serie que se queda congelada en su ultimo valor bueno.
  if (!s.link_ok) return;

  const bool env_fresh =
      s.env_seen && (uint32_t)(now - s.last_env_ms) <= SB_ENV_STALE_MS;

  // Las TRES posiciones crudas, tal como llegan. El valor que gobierna el lazo
  // sale por Air_temp; estas son las lecturas de las que sale, y la unica
  // forma de ver en remoto que una posicion se desvia de sus companeras.
  if (env_fresh) {
    static const char *const kTempKeys[3] = {SB_TEMP0_KEY, SB_TEMP1_KEY,
                                             SB_TEMP2_KEY};
    static const char *const kHumKeys[3] = {SB_HUM0_KEY, SB_HUM1_KEY,
                                            SB_HUM2_KEY};
    for (int i = 0; i < 3; i++) {
      if (s.temp.valid[i]) {
        json[kTempKeys[i]] =
            round_decimals(s.temp.value[i], SB_TELEMETRY_DECIMALS);
      }
      if (s.hum.valid[i]) {
        json[kHumKeys[i]] =
            round_decimals(s.hum.value[i], SB_TELEMETRY_DECIMALS);
      }
    }
  }

  if (env_fresh && s.lux_valid) {
    json[SB_LUX_KEY] = round_decimals(s.lux, SB_TELEMETRY_DECIMALS);
  }
  if (s.sound_seen && (uint32_t)(now - s.last_sound_ms) <= SB_SOUND_STALE_MS &&
      s.dba_valid) {
    json[SB_DB_KEY] = round_decimals(s.dba, SB_TELEMETRY_DECIMALS);
  }
  if (s.door_known && (uint32_t)(now - s.last_door_ms) <= SB_DOOR_STALE_MS) {
    json[SB_DOOR_OPEN_KEY] = s.door_open;
  }
  json[SB_ENV_USED_KEY] = env_used;
  json[SB_DOOR_FAULT_KEY] = s.door_faulty;
}

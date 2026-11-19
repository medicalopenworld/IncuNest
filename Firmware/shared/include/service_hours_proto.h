#pragma once
// Protocolo de avisos de servicio por horas de uso (Firmware/PROTOCOL.md,
// seccion "Service hours").
//
// Vive en shared/ porque lo hablan LAS DOS placas: la motherBoard construye
// `CTRL,SERVICE` y parsea `HMI,SERVICE_DONE`/`HMI,SERVICE_REQ`; el HMI hace lo
// contrario. Una sola definicion de los ids y del formato evita que un lado
// cambie y el otro siga hablando el protocolo viejo.
//
// Sin I/O ni Arduino: las funciones reciben la linea ya separada (sin '\n') y
// devuelven false/0 ante cualquier malformacion. Una linea mala se descarta
// entera, nunca se actua sobre datos a medias (.claude/rules/security.md).
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Actuadores con vida util acotada. El valor es el id que viaja por el cable:
// NO reordenar ni reutilizar un numero, solo anadir al final.
enum ServiceActuator : uint8_t {
  SVC_HEATER = 0,
  SVC_FAN = 1,
  SVC_HUMIDIFIER = 2,
  SVC_PHOTO = 3,
  SVC_COUNT = 4,
};

struct ServiceEntry {
  uint8_t id;          // ServiceActuator
  uint32_t hours;      // horas de uso desde el ultimo servicio (enteras)
  uint32_t thresholdH; // umbral de servicio, 0 = sin aviso para ese actuador
};

struct ServiceReport {
  uint8_t n;                       // entradas validas, 0..SVC_COUNT
  ServiceEntry entry[SVC_COUNT];
};

// HMI -> MB: "dame el estado de servicio ahora".
#define SERVICE_PROTO_REQ_LINE "HMI,SERVICE_REQ\n"

// CTRL,SERVICE,<n>{,<id>,<horas>,<umbral>}xn\n
// Devuelve los caracteres escritos (sin contar el '\0'), 0 si no cabe en buf o
// si el informe es incoherente (n > SVC_COUNT, id fuera de rango).
int service_proto_build_report(char *buf, size_t len, const ServiceReport *r);

// Parseo de lo anterior (la linea sin '\n'). Exige n coherente con el numero
// de campos, ids validos y sin repetir, y solo digitos. En caso de fallo deja
// `out->n = 0`.
bool service_proto_parse_report(const char *line, ServiceReport *out);

// HMI,SERVICE_DONE,<id>\n
int service_proto_build_done(char *buf, size_t len, uint8_t id);
bool service_proto_parse_done(const char *line, uint8_t *id);

// "HMI,SERVICE_REQ" exacto (la linea sin '\n').
bool service_proto_is_req(const char *line);

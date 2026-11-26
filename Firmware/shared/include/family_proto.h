#pragma once
// Protocolo del aviso "Coge mi mano" para familias (Firmware/PROTOCOL.md,
// seccion "Family notice").
//
// Vive en shared/ porque lo hablan LAS DOS placas: la motherBoard construye
// `CTRL,FAMILY` y el HMI lo parsea. Sin I/O ni Arduino: una linea mal formada
// se descarta entera, nunca se actua sobre datos a medias.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Longitud maxima de la URL (sin contar el '\0').
#define FAMILY_URL_MAX 96

struct FamilyMsg {
  bool notice;                 // hay aviso activo
  char url[FAMILY_URL_MAX + 1];  // URL del QR; "" = sin QR
};

// URL valida: empieza por "https://", 9..FAMILY_URL_MAX caracteres, solo ASCII
// imprimible 0x21..0x7E y sin coma (viaja en una linea CSV).
bool family_url_valid(const char *url);

// CTRL,FAMILY,<0|1>,<url>\n  (url puede ir vacia).
// Devuelve los caracteres escritos (sin contar el '\0'), 0 si no cabe en buf o
// si la url no es vacia ni valida.
int family_proto_build(char *buf, size_t len, const FamilyMsg *m);

// Parseo de lo anterior (la linea sin '\n'). Rechaza flag distinto de 0/1,
// campos de menos o de mas y una url no vacia que no sea valida. En fallo deja
// `out->notice = false` y `out->url[0] = '\0'`.
bool family_proto_parse(const char *line, FamilyMsg *out);

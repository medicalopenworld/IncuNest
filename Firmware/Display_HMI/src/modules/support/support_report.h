#pragma once
// Informe de depuracion para el contacto con soporte (spec hmi-help-center,
// cambio hmi-boton-ayuda).
//
// El contacto es un QR mailto: que el operador escanea con su movil; el correo
// sale de su cuenta, no del equipo. El asunto lleva el numero de serie y el
// cuerpo este informe: texto ASCII plano en lineas "clave=valor" separadas
// por '\n', acotado a SUPPORT_REPORT_MAX bytes para que quepa en el QR.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Presupuestos. El QR mailto: con ECC MEDIUM y canvas de 340 px admite ~900
// bytes ya codificados; destinatario + asunto + informe caben con margen
// (design.md, decision 3).
#define SUPPORT_REPORT_MAX 400
#define SUPPORT_SUBJECT_MAX 64
#define SUPPORT_MAILTO_MAX 1200

// Informe de depuracion del estado actual. Devuelve los bytes escritos (sin
// el terminador). Nunca escribe mas de `cap` y siempre termina en '\0'.
size_t support_report_build(char *out, size_t cap);

// Asunto: "IncuNest SN 0042 - Solicitud de soporte". El numero de serie va
// delante porque es la clave con la que soporte cruza inventario y
// dispositivo de ThingsBoard.
size_t support_report_subject(char *out, size_t cap);

// URI mailto: completo (destinatario SUPPORT_EMAIL, asunto y cuerpo
// percent-encoded, RFC 3986). `withReport` a false deja solo destinatario y
// asunto, para cuando el movil no lee el QR denso. Devuelve los bytes
// escritos, o 0 si no cabe en `cap`.
size_t support_report_build_mailto(char *out, size_t cap, bool withReport);

// URI mailto: generico: "mailto:<to>?subject=<asunto>&body=<cuerpo>" con
// asunto y cuerpo percent-encoded (`to` va tal cual). Lo usa el certificado
// de los cursos de formacion. Devuelve los bytes escritos, o 0 si no cabe.
size_t mailto_build(char *out, size_t cap, const char *to, const char *subject,
                    const char *body);

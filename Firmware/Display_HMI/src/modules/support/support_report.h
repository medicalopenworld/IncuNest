#pragma once
// Informe de depuracion y peticion de soporte del display (spec
// hmi-help-center, cambio hmi-boton-ayuda).
//
// Un unico formateador (support_report_build) alimenta las dos vias de
// contacto —telemetria ThingsBoard y QR mailto:— para que soporte reciba
// exactamente lo mismo por cualquiera de ellas. Texto ASCII plano en lineas
// "clave=valor" separadas por '\n', acotado a SUPPORT_REPORT_MAX bytes para
// que quepa en el QR y en MAX_MESSAGE_SIZE del SDK de ThingsBoard.
//
// La peticion pendiente es el puente entre la UI (que la crea bajo el mutex
// de LVGL) y la tarea WiFi/OTA (que la publica): nada de red en un callback
// de LVGL (ARQ-LOCK-001).
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Presupuestos. El QR mailto: con ECC MEDIUM y canvas de 360 px admite ~900
// bytes ya codificados; mensaje + informe + asunto + destinatario caben
// con margen (design.md, decision 3).
#define SUPPORT_REPORT_MAX 400
#define SUPPORT_MESSAGE_MAX 160
#define SUPPORT_SUBJECT_MAX 64
#define SUPPORT_MAILTO_MAX 1200
// Tiempo que la UI espera la confirmacion de la tarea WiFi antes de dar el
// envio por fallido y ofrecer el QR.
#define SUPPORT_SEND_TIMEOUT_MS 15000

// Informe de depuracion del estado actual. Devuelve los bytes escritos (sin
// el terminador). Nunca escribe mas de `cap` y siempre termina en '\0'.
size_t support_report_build(char *out, size_t cap);

// Asunto recomendado: "IncuNest SN 0042 - Solicitud de soporte". El numero de
// serie va delante porque es la clave con la que soporte cruza inventario y
// dispositivo de ThingsBoard.
size_t support_report_subject(char *out, size_t cap);

// URI mailto: completo (destinatario SUPPORT_EMAIL, asunto y cuerpo
// percent-encoded, RFC 3986). `withMessage`/`withReport` permiten degradar el
// contenido cuando no cabe en el QR. Devuelve los bytes escritos, o 0 si no
// cabe en `cap`.
size_t support_report_build_mailto(char *out, size_t cap, const char *msg,
                                   bool withMessage, bool withReport);

// ---- Peticion pendiente (UI -> tarea WiFi/OTA) ------------------------
typedef enum {
  SUPPORT_IDLE = 0,
  SUPPORT_PENDING,  // encolada, aun sin publicar
  SUPPORT_SENT,     // publicada con exito en ThingsBoard
  SUPPORT_FAILED,   // la publicacion fallo
} SupportRequestState;

// UI: toma una instantanea del informe y encola el mensaje. Sustituye a
// cualquier peticion anterior que siguiera pendiente.
void SupportRequest_Submit(const char *msg);
SupportRequestState SupportRequest_GetState(void);
void SupportRequest_Reset(void);

// Tarea WiFi/OTA: copia la peticion pendiente (si la hay) a los buffers del
// llamador, devuelve true y deja en `seq` el numero de esa peticion. El estado
// sigue en PENDING hasta SetResult(). SetResult() solo se aplica si `seq` es
// aun la peticion vigente: un resultado tardio de una peticion que la UI ya
// descarto (timeout) o sustituyo (reenvio) no debe marcar la nueva.
bool SupportRequest_TakePending(char *subject, size_t subjectCap, char *msg,
                                size_t msgCap, char *report, size_t reportCap,
                                uint32_t *seq);
void SupportRequest_SetResult(bool ok, uint32_t seq);

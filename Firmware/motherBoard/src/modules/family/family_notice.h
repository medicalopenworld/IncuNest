#pragma once
// Nucleo del aviso "Coge mi mano" para familias (Firmware/PROTOCOL.md, seccion
// "Family notice"): decide si el aviso esta activo y con que URL de QR.
//
// Logica pura a proposito, hermana de time_source: sin Arduino, sin FreeRTOS y
// sin time(). La hora llega como parametro (`nowMs` de millis(), `nowEpochS`
// del reloj de pared), asi que entra en los tests de host y el envoltorio
// seguro entre hilos (family_state) lo llama desde los callbacks de
// ThingsBoard y la tarea de la UART sin que este modulo sepa nada de eso.
#include <stdbool.h>
#include <stdint.h>

#include "family_proto.h" // FamilyMsg, family_url_valid

#ifdef __cplusplus
extern "C" {
#endif

// Tope del aviso con hora de red, aunque `hand_hold_until` pida mas.
#define FAMILY_HOLD_MAX_S (30u * 60u)
// Ventana fija cuando la hora no es de red: desde que llega el valor.
#define FAMILY_HOLD_UNTRUSTED_S (5u * 60u)

struct FamilyNotice {
  bool active;           // hay un plazo vigente (o vencido, aun sin recoger)
  uint32_t deadlineMs;   // fin del plazo en millis(); resta con signo, sin 0 magico
  double lastApplied;    // ultimo `hand_hold_until` visto (ms epoch), 0 = ninguno
  char url[FAMILY_URL_MAX + 1];
  FamilyMsg lastSent;    // ultimo mensaje devuelto por take_msg
  bool everSent;
};

void family_notice_init(FamilyNotice *f);

// Llega `hand_hold_until` (epoch en ms). `fromRequest` = es la respuesta a
// nuestra peticion de reconexion, no un cambio en vivo. Hora fiable =
// `clockFromNetwork && nowEpochS != 0`.
void family_notice_on_hand_hold(FamilyNotice *f, double untilEpochMs, bool fromRequest,
                                uint32_t nowEpochS, bool clockFromNetwork, uint32_t nowMs);

// NULL o URL invalida = sin QR (cadena vacia).
void family_notice_set_url(FamilyNotice *f, const char *url);

// Alta confirmada: aviso apagado y URL olvidada. No reinicia `lastApplied`.
void family_notice_on_discharge(FamilyNotice *f);

// True si hay que mandar `*out`: el estado visible a `nowMs` (aviso activo, URL)
// difiere del ultimo mandado, o `force`. Si no, `*out` no se toca.
bool family_notice_take_msg(FamilyNotice *f, uint32_t nowMs, bool force, FamilyMsg *out);

#ifdef __cplusplus
}
#endif

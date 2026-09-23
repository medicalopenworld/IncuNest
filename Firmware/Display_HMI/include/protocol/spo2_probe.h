#pragma once

// Estado de contacto de la sonda de SpO2 tal como llega en "CTRL,PROBE,<n>".
//
// El contrato numerico NO es de este proyecto: lo fija ProbeState de la
// libreria incunest_afe4490, y la motherboard reenvia el valor crudo. Por eso
// este enum es un espejo de aquel, valor por valor, y no se reutiliza el de la
// sonda de piel (SkinProbeState), como se hacia hasta la 4.2.1: los numeros
// coincidian por casualidad hasta 3, y el 4 de aqui habria sido
// SKIN_PROBE_OUT_OF_RANGE alli.
//
// Historia del contrato (libreria):
//   v0.81  0 DISCONNECTED, 1 NOT_APPLIED, 2 APPLIED, 3 SATURATING
//   v0.90  1 pasa a llamarse OT_HIGH y 3 AMB_SATURATING (mismos valores), y
//          aparece 4 ONLY_LED_SATURATING.
//
// En esta placa, con el HGAC encendido, 4 es el estado HABITUAL de "sonda
// retirada": sin dedo los LED saturan y el canal ambiente queda limpio. 3 exige
// ademas que sature el ambiente (luz externa, p. ej. la lampara de
// fototerapia). Antes de v0.90 ese mismo caso fisico se reportaba como 1.
//
// Tipo con base fija (uint8_t): cualquier byte es un valor valido del enum, asi
// que guardar lo que venga por el cable nunca es comportamiento indefinido.

#include <stdint.h>

enum Spo2ProbeState : uint8_t {
  SPO2_PROBE_DISCONNECTED        = 0,  // cable o conector de la sonda sin enchufar
  SPO2_PROBE_OT_HIGH             = 1,  // sonda conectada, sin tejido en el camino optico
  SPO2_PROBE_APPLIED             = 2,  // sonda en el paciente: medida normal
  SPO2_PROBE_AMB_SATURATING      = 3,  // satura tambien el ambiente: presencia DESCONOCIDA
  SPO2_PROBE_ONLY_LED_SATURATING = 4,  // satura solo la fase LED: sonda retirada
};

// Nombres anteriores a v0.90, que es como los sigue llamando parte del codigo
// y de la documentacion. Mismos valores.
static const Spo2ProbeState SPO2_PROBE_NOT_APPLIED = SPO2_PROBE_OT_HIGH;
static const Spo2ProbeState SPO2_PROBE_SATURATING  = SPO2_PROBE_AMB_SATURATING;

// Valor del cable -> estado. FAIL-SAFE: un valor que esta version no conozca
// NUNCA se descarta ni se deja pasar como APPLIED; se trata como "sin contacto
// valido" (NOT_APPLIED). Descartarlo dejaria en pie el ultimo APPLIED y con el
// la traza PPG congelada en la pantalla de bloqueo, que es el fallo que se
// corrigio cuando el 3 llegaba y se rechazaba por estar fuera de 0..2.
static inline Spo2ProbeState spo2ProbeFromWire(int v) {
  if (v < SPO2_PROBE_DISCONNECTED || v > SPO2_PROBE_ONLY_LED_SATURATING) {
    return SPO2_PROBE_NOT_APPLIED;
  }
  return (Spo2ProbeState)v;
}

// Solo APPLIED habilita la traza y las constantes vitales.
static inline bool spo2ProbeIsApplied(Spo2ProbeState s) {
  return s == SPO2_PROBE_APPLIED;
}

// Nombre corto, para logs y /debug/state. Siempre devuelve algo imprimible.
static inline const char *spo2ProbeName(Spo2ProbeState s) {
  switch (s) {
    case SPO2_PROBE_DISCONNECTED:        return "DISCONNECTED";
    case SPO2_PROBE_OT_HIGH:             return "OT_HIGH";
    case SPO2_PROBE_APPLIED:             return "APPLIED";
    case SPO2_PROBE_AMB_SATURATING:      return "AMB_SATURATING";
    case SPO2_PROBE_ONLY_LED_SATURATING: return "ONLY_LED_SATURATING";
  }
  return "UNKNOWN";
}

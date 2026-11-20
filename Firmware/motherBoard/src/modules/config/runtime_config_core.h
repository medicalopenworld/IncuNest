#pragma once
// Orquestacion de la configuracion remota SIN hardware (openspec:
// mb-remote-config). Es la capa que decide QUE se aplica, QUE se persiste y
// CUANDO, separada de NVS, de in3 y de FreeRTOS por una tabla de funciones
// (RcfgPlatform) para poder probarla en host con una NVS simulada. La
// implementacion real de la tabla vive en runtime_config.cpp.
//
// REGLAS QUE ESTA CAPA GARANTIZA (y que los tests de host comprueban):
//
//  1. NVS solo ve lo APLICADO. Un valor remoto rechazado, ignorado por la lista
//     blanca o que pierde contra un cambio local se recuerda SOLO en RAM
//     (para no repetir el aviso en cada reenvio) y no escribe nada: cualquiera
//     en la red puede publicar atributos en claro, y cada escritura de NVS
//     desgasta la particion de 20 KB que comparten mb_cfg y mb_cal.
//  2. Lo aplicado por un origen REMOTO se aplica en RAM al momento, pero se
//     persiste a traves de un cubo de fichas global (RCFG_NVS_BUCKET_CAP de
//     capacidad, una ficha nueva cada RCFG_NVS_REFILL_MS). Lo que no tiene
//     ficha queda "sucio" y se persiste cuando la haya (rcfg_poll). Un
//     reinicio antes de eso pierde el cambio, pero el ultimo valor remoto
//     persistido no cambio, asi que la siguiente peticion de atributos lo
//     vuelve a traer.
//  3. Un origen local (web, USB) persiste al momento: es una persona, no un
//     atacante en bucle, y su cambio manda sobre el remoto.
//  4. Un valor rechazado NO levanta el cambio local (override); solo lo levanta
//     una escritura remota nueva que se aplica o se aplaza.
//  5. Los rechazos repetidos de una clave se avisan como mucho una vez por
//     RCFG_REJECT_NOTE_MS y se cuentan los suprimidos.

#include <stddef.h>
#include <stdint.h>

#include "modules/config/runtime_config_rules.h"

// Version del blob de estado que se guarda en NVS. Un blob con otra version se
// ignora (estado limpio): asi un cambio futuro del formato no se interpreta con
// el layout viejo.
#define RCFG_STATE_VERSION 1

// Cubo de fichas de las escrituras de NVS de origen remoto.
#define RCFG_NVS_BUCKET_CAP 3
#define RCFG_NVS_REFILL_MS 60000UL

// Un aviso de rechazo por clave y minuto.
#define RCFG_REJECT_NOTE_MS 60000UL

typedef enum {
  RCFG_ORIGIN_USB = 0,
  RCFG_ORIGIN_WEB,
  RCFG_ORIGIN_TB_GPRS,
  RCFG_ORIGIN_TB_WIFI
} RuntimeConfigOrigin;

// Estado persistente por clave (12 B, un blob en NVS).
typedef struct {
  float lastRemote;       // ultimo valor remoto APLICADO
  uint8_t hasRemote;      // lastRemote es valido
  uint8_t overrideLocal;  // un cambio local posterior gana al reaplicado
  uint8_t src;            // origen del ultimo cambio efectivo + 1 (0 = ninguno)
  uint8_t version;        // RCFG_STATE_VERSION
  uint32_t epoch;         // cuando (0 = reloj sin sincronizar)
} RcfgKeyState;

typedef enum {
  RCFG_EV_CHANGED = 0,   // valor cambiado (auditoria, eco, log)
  RCFG_EV_UNCHANGED,     // aceptado, ya valia (eco, log de bajo nivel)
  RCFG_EV_REJECTED,      // rechazado (aviso limitado, cfg_rejected, eco)
  RCFG_EV_DEFERRED,      // aplazado (terapia activa o ritmo)
  RCFG_EV_LOCAL_WINS,    // el cambio local gana al valor remoto (aviso limitado)
  RCFG_EV_PERSIST_FAIL   // la NVS no acepto una escritura
} RcfgEventType;

typedef struct {
  RcfgEventType type;
  RuntimeConfigOrigin origin;
  RuntimeConfigId id;
  float requested;
  float applied;
  float before;
  RuntimeConfigStatus status;  // para REJECTED
  uint32_t epoch;
  uint32_t suppressed;         // avisos de esta clave callados desde el ultimo
} RcfgEvent;

// Todo lo que toca el mundo real. Ninguno puede ser NULL.
typedef struct {
  float (*current)(RuntimeConfigId id);
  // Cambia el valor en RAM (in3, efectos laterales) SIN tocar NVS.
  void (*apply_ram)(RuntimeConfigId id, float value);
  // Escribe un valor en su clave de NVS. false si la NVS falla.
  bool (*persist_value)(RuntimeConfigId id, float value);
  // Escribe el blob de estado de una clave. false si la NVS falla.
  bool (*persist_state)(RuntimeConfigId id, const RcfgKeyState *state);
  uint32_t (*now_ms)(void);
  uint32_t (*epoch)(void);
  bool (*therapy_active)(void);
  void (*context)(RuntimeConfigRemoteCtx *ctx);
  void (*notify)(const RcfgEvent *ev);
} RcfgPlatform;

typedef struct {
  float value;
  RuntimeConfigOrigin origin;
  bool valid;
} RcfgPending;

typedef struct {
  RcfgPlatform pf;
  bool safetyRemote;  // RCFG_REMOTE_SAFETY_PARAMS

  RcfgKeyState st[RCFG_COUNT];
  // Solo RAM: ultimo valor remoto visto que NO se aplico (rechazado, ignorado o
  // perdedor frente a un cambio local).
  float seen[RCFG_COUNT];
  bool seenValid[RCFG_COUNT];

  RcfgPending pending[RCFG_COUNT];

  uint32_t lastRemoteChangeMs[RCFG_COUNT];
  bool hasLastRemoteChange[RCFG_COUNT];

  // Persistencia diferida de lo aplicado por un origen remoto.
  bool dirtyValue[RCFG_COUNT];
  bool dirtyState[RCFG_COUNT];
  uint32_t tokens;
  uint32_t lastRefillMs;
  bool bucketStarted;
  uint32_t remoteNvsFlushes;  // diagnostico: veces que se uso una ficha

  // Limitador de avisos de rechazo.
  uint32_t lastNoteMs[RCFG_COUNT];
  bool hasNote[RCFG_COUNT];
  uint32_t suppressed[RCFG_COUNT];
} RcfgCore;

void rcfg_init(RcfgCore *c, const RcfgPlatform *pf, bool safety_remote);

// Carga el estado persistido de una clave. Un blob con otra version se ignora.
void rcfg_load_state(RcfgCore *c, RuntimeConfigId id, const RcfgKeyState *s);

// Origen local (web, USB): regla estricta, persiste antes de cambiar la RAM y
// marca el cambio local. Devuelve el estado de la validacion; si es aceptado,
// *persisted dice si la NVS acepto la escritura (false = no se cambio nada).
RuntimeConfigStatus rcfg_apply_local(RcfgCore *c, RuntimeConfigOrigin origin,
                                     RuntimeConfigId id, float requested,
                                     float *applied, bool *persisted);

// Una clave de un objeto de atributos de ThingsBoard. `new_write` = llega por
// la suscripcion (escritura remota nueva) y no por la respuesta a la peticion.
void rcfg_remote_key(RcfgCore *c, RuntimeConfigOrigin origin, RuntimeConfigId id,
                     float requested, bool new_write);

// Un valor remoto con tipo equivocado (cadena, booleano fuera de fan_pid_en...):
// pasa por el mismo limitador de avisos que cualquier rechazo.
void rcfg_remote_bad_type(RcfgCore *c, RuntimeConfigOrigin origin,
                          RuntimeConfigId id);

// Aplica lo aplazado que ya toca y persiste lo sucio si hay ficha.
void rcfg_poll(RcfgCore *c);

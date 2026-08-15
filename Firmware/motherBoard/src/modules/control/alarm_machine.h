#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "alarm_ids.h"
#include "alarm_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  ALARM_STATE_INACTIVE = 0,
  ALARM_STATE_PENDING,   // condicion presente, dentro del retardo de anuncio
  ALARM_STATE_ACTIVE,    // anunciandose: visual + audio
  ALARM_STATE_SILENCED,  // audio inactivo por accion del operador, visual sigue
  ALARM_STATE_ACKED,     // audio inactivo indefinidamente, visual sigue
} AlarmState;

// Duracion minima de audio que 6.10 exige completar aunque la condicion se
// haya ido: una rafaga entera en MEDIA, media rafaga en ALTA. Los valores
// salen de la Tabla 3 con el patron elegido en la spec §8.
#define ALARM_MIN_BURST_MS_HIGH   1200u
#define ALARM_MIN_BURST_MS_MEDIUM 1600u

void alarm_machine_init(void);

// Informa de si la condicion fisica esta presente. Idempotente.
void alarm_machine_condition(AlarmId id, bool present, uint32_t now_ms);

// Hace avanzar los temporizadores. Debe llamarse periodicamente.
void alarm_machine_tick(uint32_t now_ms);

// Retardo de anuncio por condicion. 201.12.3.104 lo permite hasta 30 min
// mientras la incubadora calienta desde frio. Los cortes termicos lo ignoran.
void alarm_machine_set_announce_delay(AlarmId id, uint32_t delay_ms);

// true si hay alguna condicion en ACTIVE. SILENCED y ACKED no cuentan.
bool alarm_machine_audio_required(void);

AlarmState alarm_machine_state(AlarmId id);

// Bit por AlarmId de las condiciones que estan generando senal visual, en
// cualquiera de los estados anunciables (ACTIVE, SILENCED, ACKED, PENDING).
uint32_t alarm_machine_bitmask(void);

// true si alguna condicion presente exige desconectar el calefactor.
bool alarm_machine_heater_must_cut(void);

// Inactiva el audio de UNA condicion durante duration_ms. 6.8.1 exige que no
// afecte a las senales de las demas, por eso no existe un silencio global.
void alarm_machine_silence(AlarmId id, uint32_t duration_ms, uint32_t now_ms);

// Inactiva el audio de UNA condicion por tiempo indefinido. La senal visual
// se mantiene mientras la condicion persista.
void alarm_machine_ack(AlarmId id, uint32_t now_ms);

// true si la alarma sigue senalizando solo porque es latching y su condicion
// ya desaparecio: esta esperando reset manual.
bool alarm_machine_is_latched(AlarmId id);

// Reset manual. Devuelve false si la alarma no es latching o si su condicion
// sigue presente — resetear con la causa viva no puede apagar el aviso.
bool alarm_machine_reset(AlarmId id, uint32_t now_ms);

// Prioridad mas alta entre las condiciones que se estan anunciando
// (ACTIVE, SILENCED o ACKED). Si no hay ninguna, devuelve ALARM_PRIORITY_LOW.
AlarmPriority alarm_machine_top_priority(void);

// true si alguna condicion esta generando senal visual.
bool alarm_machine_any_signalling(void);

#ifdef __cplusplus
}
#endif

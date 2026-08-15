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
// haya ido: una rafaga entera en MEDIA, media rafaga en ALTA.
//
// ATADAS al patron de pulsos que reproduce buzzerAlarmUpdate() (Buzzer.cpp)
// con las constantes de main.h. No se pueden tocar por separado: nacieron en
// tareas distintas y divergieron una vez ya (ALTA valia 1200 ms, que cortaba
// el audio tras el CUARTO pulso de los cinco que exige 6.10).
//
//   ALARM_PULSE_MS = 150, espaciado x = 100 (ALTA) e y = 200 (MEDIA).
//   ALTA:  media rafaga = 5 pulsos -> 5*150 + 4*100 = 1150 ms.
//   MEDIA: rafaga entera = 3 pulsos -> 3*150 + 2*200 =  850 ms.
//
// Los valores de abajo cubren esas duraciones con margen. Ya divergieron una
// vez (ALTA valia 1200 ms, que cortaba el audio tras el CUARTO pulso de los
// cinco que exige 6.10), asi que desde entonces Buzzer.cpp lleva static_assert
// que fallan la compilacion si el patron y estos numeros dejan de cuadrar:
// no hace falta acordarse de rehacer la cuenta, la compilacion avisa.
#define ALARM_MIN_BURST_MS_HIGH   1300u
#define ALARM_MIN_BURST_MS_MEDIUM 1000u

void alarm_machine_init(void);

// Informa de si la condicion fisica esta presente. Idempotente.
void alarm_machine_condition(AlarmId id, bool present, uint32_t now_ms);

// Hace avanzar los temporizadores. Debe llamarse periodicamente.
void alarm_machine_tick(uint32_t now_ms);

// Retardo de anuncio por condicion. 201.12.3.104 lo permite hasta 30 min
// mientras la incubadora calienta desde frio. Los cortes termicos lo ignoran.
void alarm_machine_set_announce_delay(AlarmId id, uint32_t delay_ms);

// true si hay alguna condicion en ACTIVE, o si una condicion que ya volvio a
// INACTIVE sigue dentro de su ventana de rafaga minima (6.10). SILENCED y
// ACKED no cuentan: son la inactivacion del OPERADOR que la norma exime de
// completar la rafaga, y ambas cancelan esa ventana al producirse.
//
// NO ES UNA CONSULTA PURA: al evaluar el criterio cierra de paso las ventanas
// de rafaga minima que ya se hayan consumido, igual que hace
// alarm_machine_tick(). Se lee como una pregunta y escribe estado interno, de
// ahi el aviso. No introduce carrera nueva — el unico llamante de produccion es
// driveAlarmBuzzer(), desde la misma tarea que el tick, y la via de ISR
// (ongoingAlarms() -> alarm_machine_any_signalling()) es de solo lectura—, pero
// no debe llamarse desde un contexto que no pueda escribir la maquina.
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
// Incluye SILENCED/ACKED a proposito: la senal VISUAL debe seguir mostrando
// la prioridad mas alta aunque el operador haya inactivado su audio. NO usar
// esto para decidir que patron reproduce el zumbador - ver
// alarm_machine_audible_priority().
AlarmPriority alarm_machine_top_priority(void);

// Prioridad mas alta entre las condiciones que EXIGEN AUDIO ahora mismo: el
// mismo criterio que alarm_machine_audio_required() (ACTIVE, o INACTIVE
// dentro de la ventana de rafaga minima de 6.10), pero devolviendo la
// prioridad en vez de un booleano. Si no hay ninguna, devuelve
// ALARM_PRIORITY_LOW.
//
// Existe separada de alarm_machine_top_priority() porque son dos preguntas
// distintas: esa incluye SILENCED/ACKED para la senal visual; esta no, porque
// silenciar o hacer ACK es la inactivacion del OPERADOR que 6.10 exime de
// audio. Sin esta distincion, silenciar una ALTA mientras una BAJA distinta
// sigue ACTIVE haria que el zumbador reprodujera el patron de 10 pulsos de la
// ALTA para una condicion que en realidad es BAJA - una alarma que ya no
// exige audio le presta su prioridad a otra que si lo exige.
//
// TAMPOCO ES UNA CONSULTA PURA: mismo aviso que
// alarm_machine_audio_required(), y por el mismo motivo — comparten el
// predicado, que cierra las ventanas ya consumidas.
AlarmPriority alarm_machine_audible_priority(void);

// true si alguna condicion esta generando senal visual.
bool alarm_machine_any_signalling(void);

// Bit por AlarmId de las condiciones cuyo audio esta en AUDIO PAUSED.
//
// 6.8.1 exige que el operador pueda "determinar las CONDICIONES DE ALARMA
// cuyas SENALES DE ALARMA estan inactivadas", y 201.12.3.104 que una alarma
// silenciada deliberadamente mantenga indicacion visual. Con un unico
// booleano global el operador no puede saber CUAL callo, asi que la
// informacion tiene que viajar condicion a condicion. Consulta PURA.
uint32_t alarm_machine_silenced_bitmask(void);

// Cancela el AUDIO PAUSED de UNA condicion y devuelve su audio a la vida.
// Devuelve false si esa condicion no estaba silenciada.
//
// 6.8.4: "Means shall be provided for the OPERATOR to terminate any ALARM
// SIGNAL inactivation state". Sin esto el silencio solo se podia deshacer
// esperando a que caducara el temporizador.
bool alarm_machine_unsilence(AlarmId id, uint32_t now_ms);

// true si queda algo que el operador pueda silenciar: alguna condicion en
// ACTIVE, es decir, con el audio vivo y sin inactivar todavia.
//
// Es lo que decide si el display ensena el boton de silencio. Tiene que
// venir de aqui y no de una copia local en el HMI porque la pausa de audio
// caduca sola a los 2 min (6.8.3): cuando la maquina devuelve SILENCED a
// ACTIVE y el zumbador vuelve a sonar, el display necesita enterarse para
// volver a ofrecer el boton. Con la copia local, el operador se quedaba con
// una alarma sonando y sin forma de callarla.
//
// Consulta PURA, al contrario que audio_required()/audible_priority(): no
// cierra ventanas ni toca estado, asi que puede llamarse desde cualquier
// contexto.
bool alarm_machine_any_silenceable(void);

#ifdef __cplusplus
}
#endif

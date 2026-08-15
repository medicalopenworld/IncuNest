#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "alarm_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

// Registro de las ultimas alarmas, para que quede constancia de lo que ha
// pasado en el equipo aunque nadie estuviera delante.
//
// IEC 60601-1-8 6.12.2 no obliga a tener registro, pero si lo hay exige que
// anote la ocurrencia y la identidad de TODAS las alarmas de prioridad ALTA y
// MEDIA, y para cada una la fecha y hora — o el tiempo transcurrido. Ademas
// recomienda anotar el inicio y el fin, y los limites de alarma cuando son
// ajustables por el operador, que aqui lo son (los cortes termicos).
//
// Este modulo es logica pura: no toca NVS ni Arduino. Quien lo usa le pasa el
// instante y se encarga de persistir el blob. Asi se puede testear en host,
// que es la unica forma de comprobar el envolvimiento del anillo sin esperar
// a que ocurran diez alarmas reales.

#define ALARM_HISTORY_CAPACITY 10

typedef struct {
  uint8_t id;           // AlarmId de la condicion
  uint8_t priority;     // AlarmPriority en el momento de registrarla
  uint32_t raisedEpoch;   // hora Unix UTC del alta, 0 si no habia hora
  uint32_t clearedEpoch;  // hora Unix UTC de la resolucion, 0 si sigue viva
  int16_t limitCenti;   // limite en vigor x100 (los cortes son ajustables)
  int16_t valueCenti;   // medida que la disparo x100
} AlarmHistoryEntry;

// Deja el historial vacio. No toca NVS.
void alarm_history_init(void);

// Registra un alta. Consume un hueco del anillo; al llenarse, sobrescribe la
// entrada mas antigua.
//
// limitCenti/valueCenti van multiplicados por 100 para no arrastrar coma
// flotante hasta NVS ni hasta el protocolo. Una condicion sin limite ni medida
// asociados (un fallo de ventilador, por ejemplo) pasa 0 en ambos.
void alarm_history_record_raise(AlarmId id, uint8_t priority,
                                uint32_t nowEpoch, int16_t limitCenti,
                                int16_t valueCenti);

// Sella la resolucion sobre la entrada de alta mas reciente de esa condicion
// que siga abierta. Devuelve false si no habia ninguna.
//
// Rellenar la entrada existente en vez de gastar un hueco nuevo es deliberado:
// con diez huecos, registrar alta y baja por separado hace que una alarma que
// rebota cinco veces borre todo lo anterior. Asi son diez alarmas de verdad y
// ademas se ve cuanto duro cada una.
bool alarm_history_record_clear(AlarmId id, uint32_t nowEpoch);

// Numero de entradas guardadas (0..ALARM_HISTORY_CAPACITY).
size_t alarm_history_count(void);

// Entrada por indice, 0 = la mas reciente. NULL si el indice se sale.
const AlarmHistoryEntry *alarm_history_get(size_t index);

// --- Persistencia ---
// El modulo no conoce NVS: expone el bloque de bytes y lo acepta de vuelta.
// Quien llama decide donde guardarlo.

// Tamano del blob que hay que persistir.
size_t alarm_history_blob_size(void);

// Copia el estado a buf. Devuelve los bytes escritos, 0 si buf es pequeno.
size_t alarm_history_serialize(void *buf, size_t bufLen);

// Restaura desde un blob. Devuelve false y deja el historial vacio si el blob
// no tiene el tamano o la version esperados — un formato viejo se descarta en
// vez de leerse con el paso equivocado, que decodificaria basura como si
// fueran registros clinicos.
bool alarm_history_deserialize(const void *buf, size_t bufLen);

#ifdef __cplusplus
}
#endif

#pragma once
// Contrato de fail-safe de enlace del SensorBoard (README #Protocolo):
// heartbeat cada 30 s; sin uno durante 3 periodos (90 s) el enlace se
// declara perdido. Pura funcion de (estado, now_ms) sobre el reloj LOCAL de
// la motherboard en cada heartbeat recibido -- nunca del campo "uptime" del
// propio SensorBoard, que da la vuelta a los ~49.7 dias y no es de fiar como
// referencia de tiempo ajena.
//
// Antes del primer heartbeat el enlace se considera NO disponible: no hay
// canal de diagnostico que distinga "todavia no ha enumerado" de "esta
// averiado", asi que el arranque falla-seguro igual que un timeout.
//
// Solo el heartbeat refresca el enlace, no cualquier trafico: es el unico
// mensaje periodico e incondicional (lo emite el app_main del SensorBoard),
// mientras que sensor_data o sound_level los emiten tareas de sensor que
// pueden seguir vivas con el resto del firmware degradado. Aceptar
// "cualquier trafico" convertiria esto en un detector de "el cable esta
// enchufado", que ya lo da el evento del driver USB.
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool has_seen_heartbeat;
  uint32_t last_heartbeat_ms;
} SbLinkState;

void sb_link_state_init(SbLinkState *s);

// Llamar en cada evento "heartbeat" decodificado, con el millis() local de
// la motherboard en el instante de recepcion.
void sb_link_state_note_heartbeat(SbLinkState *s, uint32_t now_ms);

// Declara el enlace caido sin esperar el timeout. Para cuando hay evidencia
// definitiva -- el dispositivo se ha ido del bus USB --: inferir durante 90 s
// algo que ya se sabe con certeza es perder senal gratis.
void sb_link_state_mark_down(SbLinkState *s);

// Evalua el enlace y, si el hueco supera el timeout, DEJA EL ESTADO CAIDO de
// forma permanente hasta el siguiente heartbeat real.
//
// NO ES UNA CONSULTA PURA, y de ahi el nombre: sin ese olvido, un enlace roto
// hace semanas volvia a declararse vivo durante 90 s cuando millis() daba la
// vuelta (~49.7 dias, uptime normal en una incubadora que no se apaga) y
// volvia a caer dentro de la ventana del ultimo heartbeat. Eso borraba la
// alarma de enlace perdido y publicaba lecturas de hace semanas como frescas.
bool sb_link_state_evaluate(SbLinkState *s, uint32_t now_ms);

// Consulta pura, para quien solo quiera leer el estado ya evaluado.
bool sb_link_state_is_connected(const SbLinkState *s, uint32_t now_ms);

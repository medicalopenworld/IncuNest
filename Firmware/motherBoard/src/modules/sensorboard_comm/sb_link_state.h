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

// Llamar periodicamente (p.ej. desde el tick de la tarea de comunicacion).
// Devuelve true si el enlace sigue vivo segun el margen de 90 s.
bool sb_link_state_is_connected(const SbLinkState *s, uint32_t now_ms);

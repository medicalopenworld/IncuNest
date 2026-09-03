#pragma once
// Reensamblado incremental de frames del SensorBoard:
// Magic(2B)+Type(1B)+Length(4B LE)+Payload(N)+CRC16(2B BE).
//
// Alimentado byte a byte (o en bloques) desde el callback RX del CDC-ACM, sin
// tocar hardware: pura, testeable en host. El buffer de payload es propiedad
// de quien llama (sensorboard_comm.cpp lo dimensiona a
// SB_PROTO_MAX_BINARY_PAYLOAD en PSRAM); este modulo nunca reserva memoria.
//
// Ante CRC invalido o un Length que no cabe en el buffer, descarta el frame y
// vuelve a buscar el magic desde el siguiente byte -- un frame corrupto no
// debe colgar el parseo de los que vengan despues.
//
// El limite del payload se comprueba EN CADA BYTE, no solo al cerrar la
// cabecera: el dueno del parser cambia payload_buf/payload_cap en caliente
// (buffer pequeno para JSON, buffer grande para una captura en vuelo) y sin
// esa comprobacion un cambio a mitad de payload escribia fuera del buffer
// nuevo. Cambiar el buffer debe hacerse SIEMPRE con
// sb_frame_parser_set_buffer(), que ademas resincroniza.
#include <stddef.h>
#include <stdint.h>

typedef void (*SbFrameOnComplete)(uint8_t type, const uint8_t *payload,
                                  uint32_t len, void *ctx);
typedef void (*SbFrameOnError)(void *ctx);

typedef enum {
  SB_FRAME_WAIT_MAGIC0 = 0,
  SB_FRAME_WAIT_MAGIC1,
  SB_FRAME_HEADER,
  SB_FRAME_PAYLOAD,
  SB_FRAME_CRC,
} SbFrameState;

typedef struct {
  SbFrameState state;

  uint8_t header[5]; // type(1) + length(4 LE), sin los 2 bytes de magic
  size_t header_idx;

  uint8_t type;
  uint32_t length;

  uint8_t *payload_buf;
  size_t payload_cap;
  size_t payload_idx;

  uint8_t crc_bytes[2];
  size_t crc_idx;
  uint16_t crc_running;

  SbFrameOnComplete on_complete;
  SbFrameOnError on_error;
  void *ctx;
} SbFrameParser;

// payload_buf/payload_cap deben cubrir el mayor frame esperado
// (SB_PROTO_MAX_BINARY_PAYLOAD para admitir tambien TYPE=0x01).
void sb_frame_parser_init(SbFrameParser *p, uint8_t *payload_buf,
                          size_t payload_cap, SbFrameOnComplete on_complete,
                          SbFrameOnError on_error, void *ctx);

// Puede llamarse con cualquier trozo de bytes, incluido uno a uno; el estado
// persiste entre llamadas.
void sb_frame_parser_feed(SbFrameParser *p, const uint8_t *data, size_t len);

// Vuelve a buscar magic, descartando el frame a medio recibir. Obligatorio al
// perder el dispositivo: un stream nuevo no puede continuar el frame del
// anterior.
void sb_frame_parser_reset(SbFrameParser *p);

// Cambia el buffer de destino y resincroniza de paso. Es la unica via
// permitida para cambiar el buffer: hacerlo a mano dejaba payload_idx
// apuntando mas alla del buffer nuevo.
void sb_frame_parser_set_buffer(SbFrameParser *p, uint8_t *payload_buf,
                                size_t payload_cap);

// Escribe un frame completo listo para enviar por el CDC. Devuelve los bytes
// escritos, o 0 si no caben en out_cap. Los comandos hacia el SensorBoard
// viajan enmarcados igual que sus eventos: mandar el JSON pelado no lo
// parsea nadie al otro lado.
size_t sb_frame_encode(uint8_t type, const uint8_t *payload, uint32_t len,
                       uint8_t *out, size_t out_cap);

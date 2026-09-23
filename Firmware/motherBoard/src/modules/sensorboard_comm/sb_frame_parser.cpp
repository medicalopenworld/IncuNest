#include "sb_frame_parser.h"

#include "sb_crc16.h"
#include "sb_protocol.h"

void sb_frame_parser_init(SbFrameParser *p, uint8_t *payload_buf,
                          size_t payload_cap, SbFrameOnComplete on_complete,
                          SbFrameOnError on_error, void *ctx) {
  p->state = SB_FRAME_WAIT_MAGIC0;
  p->header_idx = 0;
  p->type = 0;
  p->length = 0;
  p->payload_buf = payload_buf;
  p->payload_cap = payload_cap;
  p->payload_idx = 0;
  p->crc_idx = 0;
  p->crc_running = 0xFFFFu;
  p->on_complete = on_complete;
  p->on_error = on_error;
  p->ctx = ctx;
}

static void reset_to_hunt(SbFrameParser *p) {
  p->state = SB_FRAME_WAIT_MAGIC0;
  p->header_idx = 0;
  p->payload_idx = 0;
  p->crc_idx = 0;
}

void sb_frame_parser_reset(SbFrameParser *p) { reset_to_hunt(p); }

void sb_frame_parser_set_buffer(SbFrameParser *p, uint8_t *payload_buf,
                                size_t payload_cap) {
  p->payload_buf = payload_buf;
  p->payload_cap = payload_cap;
  reset_to_hunt(p);
}

// Tope por TIPO, ademas del que impone el buffer: mientras hay una captura
// armada payload_cap vale decenas de KB, y sin este limite una cabecera JSON
// con el Length corrupto se aceptaba como frame de 100 KB y el parser se
// quedaba tragando bytes durante minutos.
static bool length_fits_type(uint8_t type, uint32_t length) {
  switch (type) {
    case SB_PROTO_TYPE_JSON:
      return length <= SB_PROTO_MAX_JSON_PAYLOAD;
    case SB_PROTO_TYPE_JPEG:
      return length <= SB_PROTO_MAX_BINARY_PAYLOAD;
    default:
      return false;  // tipo desconocido: no se espera su payload
  }
}

static void feed_one(SbFrameParser *p, uint8_t byte) {
  switch (p->state) {
    case SB_FRAME_WAIT_MAGIC0:
      if (byte == SB_PROTO_MAGIC_0) {
        p->state = SB_FRAME_WAIT_MAGIC1;
      }
      break;

    case SB_FRAME_WAIT_MAGIC1:
      if (byte == SB_PROTO_MAGIC_1) {
        p->state = SB_FRAME_HEADER;
        p->header_idx = 0;
        p->crc_running = 0xFFFFu;
      } else if (byte != SB_PROTO_MAGIC_0) {
        // Deja SB_PROTO_MAGIC_0 relanzar la busqueda sin perder un byte
        // (caso 0xAB 0xAB 0xCD).
        p->state = SB_FRAME_WAIT_MAGIC0;
      }
      break;

    case SB_FRAME_HEADER:
      p->header[p->header_idx++] = byte;
      p->crc_running = sb_crc16_byte(p->crc_running, byte);
      if (p->header_idx == sizeof(p->header)) {
        p->type = p->header[0];
        p->length = (uint32_t)p->header[1] | ((uint32_t)p->header[2] << 8) |
                    ((uint32_t)p->header[3] << 16) |
                    ((uint32_t)p->header[4] << 24);
        if (p->length > p->payload_cap ||
            !length_fits_type(p->type, p->length)) {
          if (p->on_error) p->on_error(p->ctx);
          reset_to_hunt(p);
          break;
        }
        p->payload_idx = 0;
        p->state = (p->length == 0) ? SB_FRAME_CRC : SB_FRAME_PAYLOAD;
        p->crc_idx = 0;
      }
      break;

    case SB_FRAME_PAYLOAD:
      // Defensa en profundidad: si el buffer se encogio a mitad de payload
      // (captura liberada por una desconexion), aqui se corta en vez de
      // escribir fuera. sb_frame_parser_set_buffer() ya resincroniza, pero
      // esta guarda hace el desbordamiento imposible sea cual sea la
      // secuencia de cambios.
      if (p->payload_idx >= p->payload_cap) {
        if (p->on_error) p->on_error(p->ctx);
        reset_to_hunt(p);
        break;
      }
      p->payload_buf[p->payload_idx++] = byte;
      p->crc_running = sb_crc16_byte(p->crc_running, byte);
      if (p->payload_idx == p->length) {
        p->state = SB_FRAME_CRC;
        p->crc_idx = 0;
      }
      break;

    case SB_FRAME_CRC:
      p->crc_bytes[p->crc_idx++] = byte;
      if (p->crc_idx == 2) {
        uint16_t received =
            ((uint16_t)p->crc_bytes[0] << 8) | (uint16_t)p->crc_bytes[1];
        if (received == p->crc_running) {
          if (p->on_complete) {
            p->on_complete(p->type, p->payload_buf, p->length, p->ctx);
          }
        } else {
          if (p->on_error) p->on_error(p->ctx);
        }
        reset_to_hunt(p);
      }
      break;
  }
}

void sb_frame_parser_feed(SbFrameParser *p, const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    feed_one(p, data[i]);
  }
}

size_t sb_frame_encode(uint8_t type, const uint8_t *payload, uint32_t len,
                       uint8_t *out, size_t out_cap) {
  // Rechazo antes de sumar: con size_t de 32 bits en el ESP32, un len cercano
  // a 0xFFFFFFFF hacia que el total diera la vuelta y la comprobacion de
  // capacidad pasara con un buffer de 8 bytes.
  if (len > SB_PROTO_MAX_BINARY_PAYLOAD) return 0;
  const size_t total = SB_PROTO_FRAME_HEADER_SIZE + len + SB_PROTO_FRAME_CRC_SIZE;
  if (out_cap < total) return 0;

  out[0] = SB_PROTO_MAGIC_0;
  out[1] = SB_PROTO_MAGIC_1;
  out[2] = type;
  out[3] = (uint8_t)(len & 0xFF);
  out[4] = (uint8_t)((len >> 8) & 0xFF);
  out[5] = (uint8_t)((len >> 16) & 0xFF);
  out[6] = (uint8_t)((len >> 24) & 0xFF);
  for (uint32_t i = 0; i < len; i++) {
    out[SB_PROTO_FRAME_HEADER_SIZE + i] = payload[i];
  }

  // CRC sobre Type+Length+Payload, es decir todo menos los dos magic.
  const uint16_t crc =
      sb_crc16(&out[2], (size_t)(SB_PROTO_FRAME_HEADER_SIZE - 2 + len));
  out[SB_PROTO_FRAME_HEADER_SIZE + len] = (uint8_t)((crc >> 8) & 0xFF);
  out[SB_PROTO_FRAME_HEADER_SIZE + len + 1] = (uint8_t)(crc & 0xFF);
  return total;
}

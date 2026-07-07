#include "sensorBoard_frame.h"
#include "sensorBoard_crc16.h"
#include "sensorBoard_comm_protocol.h"
#include <string.h>

size_t sb_frame_encode(uint8_t type, const uint8_t *payload, size_t payload_len,
                       uint8_t *out_buf, size_t out_buf_size)
{
    size_t total = SB_PROTO_FRAME_OVERHEAD + payload_len;
    if (out_buf == NULL || out_buf_size < total) {
        return 0;
    }

    out_buf[0] = SB_PROTO_MAGIC_0;
    out_buf[1] = SB_PROTO_MAGIC_1;
    out_buf[2] = type;
    out_buf[3] = (uint8_t)(payload_len & 0xFFu);
    out_buf[4] = (uint8_t)((payload_len >> 8) & 0xFFu);
    out_buf[5] = (uint8_t)((payload_len >> 16) & 0xFFu);
    out_buf[6] = (uint8_t)((payload_len >> 24) & 0xFFu);

    if (payload_len > 0) {
        memcpy(out_buf + SB_PROTO_FRAME_HEADER_SIZE, payload, payload_len);
    }

    /* CRC sobre: type(1) + len_LE(4) + payload(N) */
    uint16_t crc = sb_crc16(out_buf + 2, 5 + payload_len);

    out_buf[SB_PROTO_FRAME_HEADER_SIZE + payload_len] = (uint8_t)(crc >> 8);
    out_buf[SB_PROTO_FRAME_HEADER_SIZE + payload_len + 1] = (uint8_t)(crc & 0xFFu);

    return total;
}

void sb_frame_dec_init(sb_frame_dec_t *dec, uint8_t *buf, size_t buf_size)
{
    dec->state = SB_DEC_MAGIC0;
    dec->payload_buf = buf;
    dec->payload_buf_size = buf_size;
    dec->payload_len = 0;
    dec->payload_pos = 0;
    dec->crc_acc = 0xFFFFu;
}

void sb_frame_dec_feed(sb_frame_dec_t *dec, uint8_t byte, sb_frame_cb_t cb, void *ctx)
{
    switch (dec->state) {

    case SB_DEC_MAGIC0:
        if (byte == SB_PROTO_MAGIC_0) {
            dec->state = SB_DEC_MAGIC1;
        }
        break;

    case SB_DEC_MAGIC1:
        if (byte == SB_PROTO_MAGIC_1) {
            dec->state = SB_DEC_TYPE;
        } else if (byte == SB_PROTO_MAGIC_0) {
            /* 0xAB 0xAB 0xCD: este byte puede ser el inicio del frame real */
            dec->state = SB_DEC_MAGIC1;
        } else {
            dec->state = SB_DEC_MAGIC0;
        }
        break;

    case SB_DEC_TYPE:
        dec->type = byte;
        dec->crc_acc = sb_crc16_byte(0xFFFFu, byte);
        dec->state = SB_DEC_LEN0;
        break;

    case SB_DEC_LEN0:
        dec->payload_len = byte;
        dec->crc_acc = sb_crc16_byte(dec->crc_acc, byte);
        dec->state = SB_DEC_LEN1;
        break;

    case SB_DEC_LEN1:
        dec->payload_len |= ((uint32_t)byte << 8);
        dec->crc_acc = sb_crc16_byte(dec->crc_acc, byte);
        dec->state = SB_DEC_LEN2;
        break;

    case SB_DEC_LEN2:
        dec->payload_len |= ((uint32_t)byte << 16);
        dec->crc_acc = sb_crc16_byte(dec->crc_acc, byte);
        dec->state = SB_DEC_LEN3;
        break;

    case SB_DEC_LEN3:
        dec->payload_len |= ((uint32_t)byte << 24);
        dec->crc_acc = sb_crc16_byte(dec->crc_acc, byte);
        if (dec->payload_len == 0) {
            dec->state = SB_DEC_CRC0;
        } else if (dec->payload_len > dec->payload_buf_size) {
            /* Longitud excesiva: descarta y vuelve a buscar magic */
            dec->state = SB_DEC_MAGIC0;
        } else {
            dec->payload_pos = 0;
            dec->state = SB_DEC_PAYLOAD;
        }
        break;

    case SB_DEC_PAYLOAD:
        dec->payload_buf[dec->payload_pos++] = byte;
        dec->crc_acc = sb_crc16_byte(dec->crc_acc, byte);
        if (dec->payload_pos >= dec->payload_len) {
            dec->state = SB_DEC_CRC0;
        }
        break;

    case SB_DEC_CRC0:
        dec->crc_rx[0] = byte;
        dec->state = SB_DEC_CRC1;
        break;

    case SB_DEC_CRC1:
        dec->crc_rx[1] = byte;
        dec->state = SB_DEC_MAGIC0;
        {
            uint16_t crc_rx = ((uint16_t)dec->crc_rx[0] << 8) | dec->crc_rx[1];
            if (crc_rx == dec->crc_acc && cb != NULL) {
                cb(dec->type, dec->payload_buf, dec->payload_len, ctx);
            }
            /* CRC inválido: descarte silencioso */
        }
        break;

    default:
        dec->state = SB_DEC_MAGIC0;
        break;
    }
}

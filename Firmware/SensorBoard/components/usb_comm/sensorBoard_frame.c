#include "sensorBoard_frame.h"
#include "sensorBoard_crc16.h"
#include "sensorBoard_comm_protocol.h"
#include <string.h>

size_t sb_frame_encode(uint8_t type, const uint8_t *payload, size_t payload_len,
                       uint8_t *out_buf, size_t out_buf_size)
{
    size_t total = SB_PROTO_FRAME_OVERHEAD + payload_len;
    if (out_buf_size < total) return 0;

    out_buf[0] = SB_PROTO_MAGIC_0;
    out_buf[1] = SB_PROTO_MAGIC_1;
    out_buf[2] = type;
    out_buf[3] = (uint8_t)( payload_len        & 0xFFu);
    out_buf[4] = (uint8_t)((payload_len >>  8) & 0xFFu);
    out_buf[5] = (uint8_t)((payload_len >> 16) & 0xFFu);
    out_buf[6] = (uint8_t)((payload_len >> 24) & 0xFFu);

    if (payload_len > 0) {
        memcpy(out_buf + SB_PROTO_FRAME_HEADER_SIZE, payload, payload_len);
    }

    /* CRC over: type(1) + len_LE(4) + payload(N) */
    uint16_t crc = sb_crc16_byte(0xFFFFu, type);
    crc = sb_crc16_byte(crc, out_buf[3]);
    crc = sb_crc16_byte(crc, out_buf[4]);
    crc = sb_crc16_byte(crc, out_buf[5]);
    crc = sb_crc16_byte(crc, out_buf[6]);
    for (size_t i = 0; i < payload_len; i++) {
        crc = sb_crc16_byte(crc, payload[i]);
    }

    out_buf[SB_PROTO_FRAME_HEADER_SIZE + payload_len]     = (uint8_t)(crc >> 8);
    out_buf[SB_PROTO_FRAME_HEADER_SIZE + payload_len + 1] = (uint8_t)(crc & 0xFFu);

    return total;
}

/* Decoder stubs — implemented in Task 5 */
void sb_frame_dec_init(sb_frame_dec_t *dec, uint8_t *buf, size_t buf_size)
{
    (void)dec; (void)buf; (void)buf_size;
}

void sb_frame_dec_feed(sb_frame_dec_t *dec, uint8_t byte,
                       sb_frame_cb_t cb, void *ctx)
{
    (void)dec; (void)byte; (void)cb; (void)ctx;
}

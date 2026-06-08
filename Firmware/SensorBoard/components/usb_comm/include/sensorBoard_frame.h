#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum {
    SB_DEC_MAGIC0=0, SB_DEC_MAGIC1, SB_DEC_TYPE,
    SB_DEC_LEN0, SB_DEC_LEN1, SB_DEC_LEN2, SB_DEC_LEN3,
    SB_DEC_PAYLOAD, SB_DEC_CRC0, SB_DEC_CRC1,
} sb_dec_state_t;

typedef struct {
    sb_dec_state_t state;
    uint8_t        type;
    uint32_t       payload_len;
    uint32_t       payload_pos;
    uint8_t       *payload_buf;
    size_t         payload_buf_size;
    uint8_t        crc_rx[2];
    uint16_t       crc_acc;
} sb_frame_dec_t;

typedef void (*sb_frame_cb_t)(uint8_t type, const uint8_t *payload, size_t len, void *ctx);

size_t sb_frame_encode(uint8_t type, const uint8_t *payload, size_t payload_len,
                       uint8_t *out_buf, size_t out_buf_size);
void sb_frame_dec_init(sb_frame_dec_t *dec, uint8_t *buf, size_t buf_size);
void sb_frame_dec_feed(sb_frame_dec_t *dec, uint8_t byte, sb_frame_cb_t cb, void *ctx);

#include "sensorBoard_frame.h"

size_t sb_frame_encode(uint8_t type, const uint8_t *payload, size_t payload_len,
                       uint8_t *out_buf, size_t out_buf_size)
{
    (void)type;
    (void)payload;
    (void)payload_len;
    (void)out_buf;
    (void)out_buf_size;
    return 0;
}

void sb_frame_dec_init(sb_frame_dec_t *dec, uint8_t *buf, size_t buf_size)
{
    (void)dec;
    (void)buf;
    (void)buf_size;
}

void sb_frame_dec_feed(sb_frame_dec_t *dec, uint8_t byte, sb_frame_cb_t cb, void *ctx)
{
    (void)dec;
    (void)byte;
    (void)cb;
    (void)ctx;
}

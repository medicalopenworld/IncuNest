#include "sensorBoard_frame.h"
size_t sb_frame_encode(uint8_t t, const uint8_t *p, size_t plen, uint8_t *out, size_t outsz)
{ (void)t;(void)p;(void)plen;(void)out;(void)outsz; return 0; }
void sb_frame_dec_init(sb_frame_dec_t *d, uint8_t *buf, size_t sz)
{ (void)d;(void)buf;(void)sz; }
void sb_frame_dec_feed(sb_frame_dec_t *d, uint8_t b, sb_frame_cb_t cb, void *ctx)
{ (void)d;(void)b;(void)cb;(void)ctx; }

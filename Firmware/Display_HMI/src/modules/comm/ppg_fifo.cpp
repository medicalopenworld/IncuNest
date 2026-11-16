#include "modules/comm/ppg_fifo.h"

static_assert(256 % PPG_FIFO_LEN == 0,
              "PPG_FIFO_LEN must divide 256: head and tail are free-running uint8_t");

bool ppg_fifo_push(PpgFifo *f, uint8_t v) {
  const uint8_t head = f->head;
  if ((uint8_t)(head - f->tail) >= PPG_FIFO_LEN) {
    f->dropped++;
    return false;
  }
  f->buf[head % PPG_FIFO_LEN] = v;
  f->head = (uint8_t)(head + 1);  // publish after the write
  return true;
}

bool ppg_fifo_pop(PpgFifo *f, uint8_t *v) {
  const uint8_t tail = f->tail;
  if (tail == f->head) return false;
  *v = f->buf[tail % PPG_FIFO_LEN];
  f->tail = (uint8_t)(tail + 1);  // free the slot after the read
  return true;
}

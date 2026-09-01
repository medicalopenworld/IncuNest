#include "sb_crc16.h"

uint16_t sb_crc16_byte(uint16_t crc, uint8_t byte) {
  crc ^= (uint16_t)byte << 8;
  for (int i = 0; i < 8; i++) {
    crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                          : (uint16_t)(crc << 1);
  }
  return crc;
}

uint16_t sb_crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc = sb_crc16_byte(crc, data[i]);
  }
  return crc;
}

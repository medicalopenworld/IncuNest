/* Interno de usb_comm; expuesto en include/ solo para los tests Unity. */
#pragma once
#include <stdint.h>
#include <stddef.h>

uint16_t sb_crc16_byte(uint16_t crc, uint8_t byte);
uint16_t sb_crc16(const uint8_t *data, size_t len);

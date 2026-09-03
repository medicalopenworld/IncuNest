#pragma once
// CRC16-CCITT FALSE (poly 0x1021, init 0xFFFF), portado literal de
// SensorBoard_v2/components/usb_comm/sensorBoard_crc16.c (ADR-0001 del
// SensorBoard). Pura: sin dependencias de hardware, testeable en host.
#include <stdint.h>
#include <stddef.h>

uint16_t sb_crc16_byte(uint16_t crc, uint8_t byte);
uint16_t sb_crc16(const uint8_t *data, size_t len);
